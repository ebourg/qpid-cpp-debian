
//
// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
// 
//   http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
//

// This source file was created by a code generator.
// Please do not edit.

#include "qpid/management/Manageable.h" 
#include "qpid/management/Buffer.h"
#include "qpid/types/Variant.h"
#include "qpid/amqp_0_10/Codecs.h"
#include "qpid/agent/ManagementAgent.h"
#include "EventChildDestroyed.h"

using namespace qmf::org::apache::qpid::agent::example;
using           qpid::management::ManagementAgent;
using           qpid::management::Manageable;
using           qpid::management::ManagementObject;
using           qpid::management::Args;
using           qpid::management::Mutex;
using           std::string;

string  EventChildDestroyed::packageName  = string ("org.apache.qpid.agent.example");
string  EventChildDestroyed::eventName    = string ("ChildDestroyed");
uint8_t EventChildDestroyed::md5Sum[16]   =
    {0x8b,0x6a,0xb5,0xf4,0x51,0x8,0xb1,0x26,0x89,0xc9,0x50,0xc0,0x97,0x9b,0xf9,0xd5};

EventChildDestroyed::EventChildDestroyed (const std::string& _childName) :
    childName(_childName)
{}

namespace {
    const string NAME("name");
    const string TYPE("type");
    const string DESC("desc");
    const string ARGCOUNT("argCount");
    const string ARGS("args");
}

void EventChildDestroyed::registerSelf(ManagementAgent* agent)
{
    agent->registerEvent(packageName, eventName, md5Sum, writeSchema);
}

void EventChildDestroyed::writeSchema (std::string& schema)
{
    const int _bufSize = 65536;
    char _msgChars[_bufSize];
    ::qpid::management::Buffer buf(_msgChars, _bufSize);
    ::qpid::types::Variant::Map ft;

    // Schema class header:
    buf.putOctet       (CLASS_KIND_EVENT);
    buf.putShortString (packageName); // Package Name
    buf.putShortString (eventName);   // Event Name
    buf.putBin128      (md5Sum);      // Schema Hash
    buf.putShort       (1); // Argument Count

    // Arguments
    ft.clear();
    ft[NAME] = "childName";
    ft[TYPE] = TYPE_LSTR;
    buf.putMap(ft);


    {
        uint32_t _len = buf.getPosition();
        buf.reset();
        buf.getRawData(schema, _len);
    }
}

void EventChildDestroyed::encode(std::string& _sBuf) const
{
    const int _bufSize=65536;
    char _msgChars[_bufSize];
    ::qpid::management::Buffer buf(_msgChars, _bufSize);

    buf.putMediumString(childName);


    uint32_t _bufLen = buf.getPosition();
    buf.reset();

    buf.getRawData(_sBuf, _bufLen);
}

void EventChildDestroyed::mapEncode(::qpid::types::Variant::Map& map) const
{
    using namespace ::qpid::types;
    map["childName"] = ::qpid::types::Variant(childName);

}

bool EventChildDestroyed::match(const std::string& evt, const std::string& pkg)
{
    return eventName == evt && packageName == pkg;
}
