/*
 * ====================================================================
 *
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 */

package ch.boye.httpclientandroidlib.impl.conn;

import java.net.InetAddress;
import java.net.InetSocketAddress;

import ch.boye.httpclientandroidlib.HttpHost;

/**
 * Extended {@link InetSocketAddress} implementation that also provides access to the original
 * {@link HttpHost} used to resolve the address.
 */
class HttpInetSocketAddress extends InetSocketAddress {

    private static final long serialVersionUID = -6650701828361907957L;

    private final HttpHost host;

    public HttpInetSocketAddress(final HttpHost host, final InetAddress addr, int port) {
        super(addr, port);
        if (host == null) {
            throw new IllegalArgumentException("HTTP host may not be null");
        }
        this.host = host;
    }

    public HttpHost getHost() {
        return this.host;
    }

    @Override
    public String toString() {
        return this.host.getHostName() + ":" + getPort();
    }

}
