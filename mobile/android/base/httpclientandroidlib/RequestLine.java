/*
 * ====================================================================
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
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 */

package ch.boye.httpclientandroidlib;

/**
 * The Request-Line begins with a method token, followed by the
 * Request-URI and the protocol version, and ending with CRLF. The
 * elements are separated by SP characters. No CR or LF is allowed
 * except in the final CRLF sequence.
 * <pre>
 *      Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
 * </pre>
 *
 * @since 4.0
 */
public interface RequestLine {

    String getMethod();

    ProtocolVersion getProtocolVersion();

    String getUri();

}
