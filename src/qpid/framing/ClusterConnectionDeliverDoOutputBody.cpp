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

///
/// This file was automatically generated from the AMQP specification.
/// Do not edit.
///


#include "qpid/framing/ClusterConnectionDeliverDoOutputBody.h"
#include "qpid/framing/Buffer.h"

using namespace qpid::framing;

void ClusterConnectionDeliverDoOutputBody::setLimit(uint32_t _limit) {
    limit = _limit;
    flags |= (1 << 8);
}
uint32_t ClusterConnectionDeliverDoOutputBody::getLimit() const { return limit; }
bool ClusterConnectionDeliverDoOutputBody::hasLimit() const { return flags & (1 << 8); }
void ClusterConnectionDeliverDoOutputBody::clearLimitFlag() { flags &= ~(1 << 8); }

void ClusterConnectionDeliverDoOutputBody::encodeStructBody(Buffer& buffer) const
{
encodeHeader(buffer);
    buffer.putShort(flags);
    if (flags & (1 << 8))
        buffer.putLong(limit);
}

void ClusterConnectionDeliverDoOutputBody::encode(Buffer& buffer) const
{
    encodeStructBody(buffer);
}

void ClusterConnectionDeliverDoOutputBody::decodeStructBody(Buffer& buffer, uint32_t /*size*/)
{
decodeHeader(buffer);
    flags = buffer.getShort();
    if (flags & (1 << 8))
        limit = buffer.getLong();
}

void ClusterConnectionDeliverDoOutputBody::decode(Buffer& buffer, uint32_t /*size*/)
{
    decodeStructBody(buffer);
}

uint32_t ClusterConnectionDeliverDoOutputBody::bodySize() const
{
    uint32_t total = 0;
total += headerSize();
    total += 2;
    if (flags & (1 << 8))
        total += 4;//limit
    return total;
}

uint32_t ClusterConnectionDeliverDoOutputBody::encodedSize() const {
    uint32_t total = bodySize();
    return total;
}

void ClusterConnectionDeliverDoOutputBody::print(std::ostream& out) const
{
    out << "{ClusterConnectionDeliverDoOutputBody: ";
    if (flags & (1 << 8))
        out << "limit=" << limit << "; ";
    out << "}";
}
