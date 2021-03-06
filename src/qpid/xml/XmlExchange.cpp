/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#include "config.h"

#include "qpid/xml/XmlExchange.h"

#include "qpid/broker/DeliverableMessage.h"

#include "qpid/log/Statement.h"
#include "qpid/broker/FedOps.h"
#include "qpid/framing/FieldTable.h"
#include "qpid/framing/FieldValue.h"
#include "qpid/framing/reply_exceptions.h"

#include "qpid/Plugin.h"

#include <xercesc/framework/MemBufInputSource.hpp>

#ifdef XQ_EFFECTIVE_BOOLEAN_VALUE_HPP
#include <xqilla/ast/XQEffectiveBooleanValue.hpp>
#endif

#include <xqilla/ast/XQGlobalVariable.hpp>

#include <xqilla/context/ItemFactory.hpp>
#include <xqilla/xqilla-simple.hpp>

#include <boost/bind.hpp>
#include <functional>
#include <algorithm>
#include <iostream>
#include <sstream>

using namespace qpid::framing;
using namespace qpid::sys;
using qpid::management::Manageable;
namespace _qmf = qmf::org::apache::qpid::broker;

namespace qpid {
namespace broker {            
    
XQilla XmlBinding::xqilla;

XmlBinding::XmlBinding(const std::string& key, const Queue::shared_ptr queue, const std::string& _fedOrigin, Exchange* parent, 
                                    const ::qpid::framing::FieldTable& _arguments, const std::string& queryText )
    :      Binding(key, queue, parent, _arguments),
           xquery(),
           parse_message_content(true),
           fedOrigin(_fedOrigin)
{ 
    startManagement();

    QPID_LOG(trace, "Creating binding with query: " << queryText );

    try {  
        Query q(xqilla.parse(X(queryText.c_str())));
        xquery = q;
        
        QPID_LOG(trace, "Bound successfully with query: " << queryText );

        parse_message_content = false;

        if (xquery->getQueryBody()->getStaticAnalysis().areContextFlagsUsed()) {
            parse_message_content = true;
        }
        else {
            GlobalVariables &vars = const_cast<GlobalVariables&>(xquery->getVariables());
            for (GlobalVariables::iterator it = vars.begin(); it != vars.end(); ++it) {
                if ((*it)->getStaticAnalysis().areContextFlagsUsed()) {
                    parse_message_content = true;
                    break;
                } 
            }
        }
    }
    catch (XQException& e) {
        throw InternalErrorException(QPID_MSG("Could not parse xquery:"+ queryText));
    }
    catch (...) {
        throw InternalErrorException(QPID_MSG("Unexpected error - Could not parse xquery:"+ queryText));
    }    
}

     
XmlExchange::XmlExchange(const std::string& _name, Manageable* _parent, Broker* b) : Exchange(_name, _parent, b)
{
    if (mgmtExchange != 0)
        mgmtExchange->set_type (typeName);
}

XmlExchange::XmlExchange(const std::string& _name, bool _durable,
                         const FieldTable& _args, Manageable* _parent, Broker* b) :
    Exchange(_name, _durable, _args, _parent, b)
{
    if (mgmtExchange != 0)
        mgmtExchange->set_type (typeName);
}
    
bool XmlExchange::bind(Queue::shared_ptr queue, const std::string& bindingKey, const FieldTable* args)
{ 

    // Federation uses bind for unbind and reorigin comands as well as for binds.
    //
    // Both federated and local binds are done in this method.  Other
    // federated requests are done by calling the relevent methods.

    std::string fedOp;
    std::string fedTags;
    std::string fedOrigin;
    
    if (args) 
        fedOp = args->getAsString(qpidFedOp);
    if (! fedOp.empty())  {
        fedTags =  args->getAsString(qpidFedTags);
        fedOrigin = args->getAsString(qpidFedOrigin);
    }

    if (fedOp == fedOpUnbind) {
        return fedUnbind(fedOrigin, fedTags, queue, bindingKey, args);
    }
    else if (fedOp == fedOpReorigin) {
        fedReorigin();
        return true;
    }

    // OK, looks like we're really going to bind
    
    else if (fedOp.empty() || fedOp == fedOpBind) {

        std::string queryText = args->getAsString("xquery");

        RWlock::ScopedWlock l(lock);   
 
        XmlBinding::vector& bindings(bindingsMap[bindingKey]);
        XmlBinding::vector::ConstPtr p = bindings.snapshot();
       
        if (!p || std::find_if(p->begin(), p->end(), MatchQueueAndOrigin(queue, fedOrigin)) == p->end()) {

            XmlBinding::shared_ptr binding(new XmlBinding (bindingKey, queue, fedOrigin, this, *args, queryText));
            bindings.add(binding);

            if (mgmtExchange != 0) {
                mgmtExchange->inc_bindingCount();
            }
        } else {
            return false;
        }
    }
    else {
        QPID_LOG(warning, "Unknown Federation Op: " << fedOp);
    }
 
    routeIVE();
    propagateFedOp(bindingKey, fedTags, fedOp, fedOrigin, args);

    return true;
}

bool XmlExchange::unbind(Queue::shared_ptr queue, const std::string& bindingKey, const FieldTable* args)
{
    /*
     *  When called directly, no qpidFedOrigin argument will be
     *  present. When called from federation, it will be present. 
     *
     *  This is a bit of a hack - the binding needs the origin, but
     *  this interface, as originally defined, would not supply one.
     */
    std::string fedOrigin;
    if (args) fedOrigin = args->getAsString(qpidFedOrigin);

    RWlock::ScopedWlock l(lock);
    if (bindingsMap[bindingKey].remove_if(MatchQueueAndOrigin(queue, fedOrigin))) {
        if (mgmtExchange != 0) {
            mgmtExchange->dec_bindingCount();
        }
        return true;
    } else {
        return false;      
    }
}

bool XmlExchange::matches(Query& query, Deliverable& msg, const qpid::framing::FieldTable* args, bool parse_message_content) 
{
    std::string msgContent;

    try {
        QPID_LOG(trace, "matches: query is [" << UTF8(query->getQueryText()) << "]");

        boost::scoped_ptr<DynamicContext> context(query->createDynamicContext());
        if (!context.get()) {
            throw InternalErrorException(QPID_MSG("Query context looks munged ..."));
        }

        if (parse_message_content) {

            msg.getMessage().getFrames().getContent(msgContent);

            QPID_LOG(trace, "matches: message content is [" << msgContent << "]");

            XERCES_CPP_NAMESPACE::MemBufInputSource xml((const XMLByte*) msgContent.c_str(), 
                                                        msgContent.length(), "input" );

            // This will parse the document using either Xerces or FastXDM, depending
            // on your XQilla configuration. FastXDM can be as much as 10x faster.

            Sequence seq(context->parseDocument(xml));

            if(!seq.isEmpty() && seq.first()->isNode()) {
                context->setContextItem(seq.first());
                context->setContextPosition(1);
                context->setContextSize(1);
            }
        }

        if (args) {
            FieldTable::ValueMap::const_iterator v = args->begin();
            for(; v != args->end(); ++v) {

                if (v->second->convertsTo<double>()) {
                    QPID_LOG(trace, "XmlExchange, external variable (double): " << v->first << " = " << v->second->get<double>());
                    Item::Ptr value = context->getItemFactory()->createDouble(v->second->get<double>(), context.get());
                    context->setExternalVariable(X(v->first.c_str()), value);
                }              
                else if (v->second->convertsTo<int>()) {
                    QPID_LOG(trace, "XmlExchange, external variable (int):" << v->first << " = " << v->second->getData().getInt());
                    Item::Ptr value = context->getItemFactory()->createInteger(v->second->get<int>(), context.get());
                    context->setExternalVariable(X(v->first.c_str()), value);
                }
                else if (v->second->convertsTo<std::string>()) {
                    QPID_LOG(trace, "XmlExchange, external variable (string):" << v->first << " = " << v->second->getData().getString().c_str());
                    Item::Ptr value = context->getItemFactory()->createString(X(v->second->get<std::string>().c_str()), context.get());
                    context->setExternalVariable(X(v->first.c_str()), value);
                }

            }
        }

        Result result = query->execute(context.get());
#ifdef XQ_EFFECTIVE_BOOLEAN_VALUE_HPP
        Item::Ptr first_ = result->next(context.get());
        Item::Ptr second_ = result->next(context.get());
        return XQEffectiveBooleanValue::get(first_, second_, context.get(), 0);
#else 
        return result->getEffectiveBooleanValue(context.get(), 0);
#endif
    }
    catch (XQException& e) {
        QPID_LOG(warning, "Could not parse XML content (or message headers):" << msgContent);
    }
    catch (...) {
        QPID_LOG(warning, "Unexpected error routing message: " << msgContent);
    }
    return 0;
}

// Future optimization: If any query in a binding for a given routing key requires
// message content, parse the message once, and use that parsed form for all bindings.
//
// Future optimization: XQilla does not currently do document projection for data
// accessed via the context item. If there is a single query for a given routing key,
// and it accesses document data, this could be a big win.
//
// Document projection often is not a win if you have multiple queries on the same data.
// But for very large messages, if all these queries are on the first part of the data,
// it could still be a big win.

void XmlExchange::route(Deliverable& msg)
{
    const std::string& routingKey = msg.getMessage().getRoutingKey();
    const FieldTable* args = msg.getMessage().getApplicationHeaders();
    PreRoute pr(msg, this);
    try {
        XmlBinding::vector::ConstPtr p;
        BindingList b(new std::vector<boost::shared_ptr<qpid::broker::Exchange::Binding> >);
        {
            RWlock::ScopedRlock l(lock);
            p = bindingsMap[routingKey].snapshot();
            if (!p.get()) return;
        }

        for (std::vector<XmlBinding::shared_ptr>::const_iterator i = p->begin(); i != p->end(); i++) {
            if (matches((*i)->xquery, msg, args, (*i)->parse_message_content)) { 
                b->push_back(*i);
            }
        }
        doRoute(msg, b);
    } catch (...) {
        QPID_LOG(warning, "XMLExchange " << getName() << ": exception routing message with query " << routingKey);
    }
}


bool XmlExchange::isBound(Queue::shared_ptr queue, const std::string* const bindingKey, const FieldTable* const) 
{
    RWlock::ScopedRlock l(lock);
    if (bindingKey) {
        XmlBindingsMap::iterator i = bindingsMap.find(*bindingKey);

        if (i == bindingsMap.end())
            return false;
        if (!queue)
            return true;
        XmlBinding::vector::ConstPtr p = i->second.snapshot();
        return p && std::find_if(p->begin(), p->end(), MatchQueue(queue)) != p->end();
    } else if (!queue) {
        //if no queue or routing key is specified, just report whether any bindings exist
        return bindingsMap.size() > 0;
    } else {
        for (XmlBindingsMap::iterator i = bindingsMap.begin(); i != bindingsMap.end(); i++) {
            XmlBinding::vector::ConstPtr p = i->second.snapshot();
            if (p && std::find_if(p->begin(), p->end(), MatchQueue(queue)) != p->end()) return true;
        }
        return false;
    }

}

XmlExchange::~XmlExchange() 
{
    bindingsMap.clear();
}

void XmlExchange::propagateFedOp(const std::string& bindingKey, const std::string& fedTags, const std::string& fedOp, const std::string& fedOrigin, const qpid::framing::FieldTable* args)
{
    FieldTable nonFedArgs;

    if (args) {            
        for (qpid::framing::FieldTable::ValueMap::const_iterator i=args->begin(); i != args->end(); ++i)  {
            const std::string& name(i->first);
            if (name != qpidFedOp &&
                name != qpidFedTags &&
                name != qpidFedOrigin)  {
                nonFedArgs.insert((*i));
            }
        }
    }

    FieldTable* propArgs  = (nonFedArgs.count() > 0 ? &nonFedArgs : 0);
    Exchange::propagateFedOp(bindingKey, fedTags, fedOp, fedOrigin, propArgs);
}

bool XmlExchange::fedUnbind(const std::string& fedOrigin, const std::string& fedTags, Queue::shared_ptr queue, const std::string& bindingKey, const FieldTable* args)
{
    RWlock::ScopedRlock l(lock);

    if (unbind(queue, bindingKey, args)) {
        propagateFedOp(bindingKey, fedTags, fedOpUnbind, fedOrigin); 
        return true;
    }
    return false;
}

void XmlExchange::fedReorigin()
{
    std::vector<std::string> keys2prop;
    {
        RWlock::ScopedRlock l(lock);   
        for (XmlBindingsMap::iterator i = bindingsMap.begin(); i != bindingsMap.end(); ++i) {
            XmlBinding::vector::ConstPtr p = i->second.snapshot();
            if (std::find_if(p->begin(), p->end(), MatchOrigin(std::string())) != p->end()) {
                keys2prop.push_back(i->first);
            }
        }
    }   /* lock dropped */
    for (std::vector<std::string>::const_iterator key = keys2prop.begin();
         key != keys2prop.end(); key++) {
        propagateFedOp( *key, std::string(), fedOpBind, std::string());
    }
}


XmlExchange::MatchOrigin::MatchOrigin(const std::string& _origin) : origin(_origin) {}

bool XmlExchange::MatchOrigin::operator()(XmlBinding::shared_ptr b)
{
    return b->fedOrigin == origin;
}


XmlExchange::MatchQueueAndOrigin::MatchQueueAndOrigin(Queue::shared_ptr _queue, const std::string& _origin) : queue(_queue), origin(_origin) {}

bool XmlExchange::MatchQueueAndOrigin::operator()(XmlBinding::shared_ptr b)
{
    return b->queue == queue and b->fedOrigin == origin;
}


const std::string XmlExchange::typeName("xml");
 
}
}
