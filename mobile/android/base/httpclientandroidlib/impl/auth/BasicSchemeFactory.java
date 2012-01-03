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

package ch.boye.httpclientandroidlib.impl.auth;

import ch.boye.httpclientandroidlib.annotation.Immutable;

import ch.boye.httpclientandroidlib.auth.AuthScheme;
import ch.boye.httpclientandroidlib.auth.AuthSchemeFactory;
import ch.boye.httpclientandroidlib.params.HttpParams;

/**
 * {@link AuthSchemeFactory} implementation that creates and initializes
 * {@link BasicScheme} instances.
 *
 * @since 4.0
 */
@Immutable
public class BasicSchemeFactory implements AuthSchemeFactory {

    public AuthScheme newInstance(final HttpParams params) {
        return new BasicScheme();
    }

}
