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

package ch.boye.httpclientandroidlib.conn.routing;

import java.net.InetAddress;

import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;
import ch.boye.httpclientandroidlib.util.LangUtils;

import ch.boye.httpclientandroidlib.HttpHost;

/**
 * Helps tracking the steps in establishing a route.
 *
 * @since 4.0
 */
@NotThreadSafe
public final class RouteTracker implements RouteInfo, Cloneable {

    /** The target host to connect to. */
    private final HttpHost targetHost;

    /**
     * The local address to connect from.
     * <code>null</code> indicates that the default should be used.
     */
    private final InetAddress localAddress;

    // the attributes above are fixed at construction time
    // now follow attributes that indicate the established route

    /** Whether the first hop of the route is established. */
    private boolean connected;

    /** The proxy chain, if any. */
    private HttpHost[] proxyChain;

    /** Whether the the route is tunnelled end-to-end through proxies. */
    private TunnelType tunnelled;

    /** Whether the route is layered over a tunnel. */
    private LayerType layered;

    /** Whether the route is secure. */
    private boolean secure;

    /**
     * Creates a new route tracker.
     * The target and origin need to be specified at creation time.
     *
     * @param target    the host to which to route
     * @param local     the local address to route from, or
     *                  <code>null</code> for the default
     */
    public RouteTracker(HttpHost target, InetAddress local) {
        if (target == null) {
            throw new IllegalArgumentException("Target host may not be null.");
        }
        this.targetHost   = target;
        this.localAddress = local;
        this.tunnelled    = TunnelType.PLAIN;
        this.layered      = LayerType.PLAIN;
    }


    /**
     * Creates a new tracker for the given route.
     * Only target and origin are taken from the route,
     * everything else remains to be tracked.
     *
     * @param route     the route to track
     */
    public RouteTracker(HttpRoute route) {
        this(route.getTargetHost(), route.getLocalAddress());
    }

    /**
     * Tracks connecting to the target.
     *
     * @param secure    <code>true</code> if the route is secure,
     *                  <code>false</code> otherwise
     */
    public final void connectTarget(boolean secure) {
        if (this.connected) {
            throw new IllegalStateException("Already connected.");
        }
        this.connected = true;
        this.secure = secure;
    }

    /**
     * Tracks connecting to the first proxy.
     *
     * @param proxy     the proxy connected to
     * @param secure    <code>true</code> if the route is secure,
     *                  <code>false</code> otherwise
     */
    public final void connectProxy(HttpHost proxy, boolean secure) {
        if (proxy == null) {
            throw new IllegalArgumentException("Proxy host may not be null.");
        }
        if (this.connected) {
            throw new IllegalStateException("Already connected.");
        }
        this.connected  = true;
        this.proxyChain = new HttpHost[]{ proxy };
        this.secure     = secure;
    }

    /**
     * Tracks tunnelling to the target.
     *
     * @param secure    <code>true</code> if the route is secure,
     *                  <code>false</code> otherwise
     */
    public final void tunnelTarget(boolean secure) {
        if (!this.connected) {
            throw new IllegalStateException("No tunnel unless connected.");
        }
        if (this.proxyChain == null) {
            throw new IllegalStateException("No tunnel without proxy.");
        }
        this.tunnelled = TunnelType.TUNNELLED;
        this.secure    = secure;
    }

    /**
     * Tracks tunnelling to a proxy in a proxy chain.
     * This will extend the tracked proxy chain, but it does not mark
     * the route as tunnelled. Only end-to-end tunnels are considered there.
     *
     * @param proxy     the proxy tunnelled to
     * @param secure    <code>true</code> if the route is secure,
     *                  <code>false</code> otherwise
     */
    public final void tunnelProxy(HttpHost proxy, boolean secure) {
        if (proxy == null) {
            throw new IllegalArgumentException("Proxy host may not be null.");
        }
        if (!this.connected) {
            throw new IllegalStateException("No tunnel unless connected.");
        }
        if (this.proxyChain == null) {
            throw new IllegalStateException("No proxy tunnel without proxy.");
        }

        // prepare an extended proxy chain
        HttpHost[] proxies = new HttpHost[this.proxyChain.length+1];
        System.arraycopy(this.proxyChain, 0,
                         proxies, 0, this.proxyChain.length);
        proxies[proxies.length-1] = proxy;

        this.proxyChain = proxies;
        this.secure     = secure;
    }

    /**
     * Tracks layering a protocol.
     *
     * @param secure    <code>true</code> if the route is secure,
     *                  <code>false</code> otherwise
     */
    public final void layerProtocol(boolean secure) {
        // it is possible to layer a protocol over a direct connection,
        // although this case is probably not considered elsewhere
        if (!this.connected) {
            throw new IllegalStateException
                ("No layered protocol unless connected.");
        }
        this.layered = LayerType.LAYERED;
        this.secure  = secure;
    }

    public final HttpHost getTargetHost() {
        return this.targetHost;
    }

    public final InetAddress getLocalAddress() {
        return this.localAddress;
    }

    public final int getHopCount() {
        int hops = 0;
        if (this.connected) {
            if (proxyChain == null)
                hops = 1;
            else
                hops = proxyChain.length + 1;
        }
        return hops;
    }

    public final HttpHost getHopTarget(int hop) {
        if (hop < 0)
            throw new IllegalArgumentException
                ("Hop index must not be negative: " + hop);
        final int hopcount = getHopCount();
        if (hop >= hopcount) {
            throw new IllegalArgumentException
                ("Hop index " + hop +
                 " exceeds tracked route length " + hopcount +".");
        }

        HttpHost result = null;
        if (hop < hopcount-1)
            result = this.proxyChain[hop];
        else
            result = this.targetHost;

        return result;
    }

    public final HttpHost getProxyHost() {
        return (this.proxyChain == null) ? null : this.proxyChain[0];
    }

    public final boolean isConnected() {
        return this.connected;
    }

    public final TunnelType getTunnelType() {
        return this.tunnelled;
    }

    public final boolean isTunnelled() {
        return (this.tunnelled == TunnelType.TUNNELLED);
    }

    public final LayerType getLayerType() {
        return this.layered;
    }

    public final boolean isLayered() {
        return (this.layered == LayerType.LAYERED);
    }

    public final boolean isSecure() {
        return this.secure;
    }

    /**
     * Obtains the tracked route.
     * If a route has been tracked, it is {@link #isConnected connected}.
     * If not connected, nothing has been tracked so far.
     *
     * @return  the tracked route, or
     *          <code>null</code> if nothing has been tracked so far
     */
    public final HttpRoute toRoute() {
        return !this.connected ?
            null : new HttpRoute(this.targetHost, this.localAddress,
                                 this.proxyChain, this.secure,
                                 this.tunnelled, this.layered);
    }

    /**
     * Compares this tracked route to another.
     *
     * @param o         the object to compare with
     *
     * @return  <code>true</code> if the argument is the same tracked route,
     *          <code>false</code>
     */
    @Override
    public final boolean equals(Object o) {
        if (o == this)
            return true;
        if (!(o instanceof RouteTracker))
            return false;

        RouteTracker that = (RouteTracker) o;
        return
            // Do the cheapest checks first
            (this.connected == that.connected) &&
            (this.secure    == that.secure) &&
            (this.tunnelled == that.tunnelled) &&
            (this.layered   == that.layered) &&
            LangUtils.equals(this.targetHost, that.targetHost) &&
            LangUtils.equals(this.localAddress, that.localAddress) &&
            LangUtils.equals(this.proxyChain, that.proxyChain);
    }

    /**
     * Generates a hash code for this tracked route.
     * Route trackers are modifiable and should therefore not be used
     * as lookup keys. Use {@link #toRoute toRoute} to obtain an
     * unmodifiable representation of the tracked route.
     *
     * @return  the hash code
     */
    @Override
    public final int hashCode() {
        int hash = LangUtils.HASH_SEED;
        hash = LangUtils.hashCode(hash, this.targetHost);
        hash = LangUtils.hashCode(hash, this.localAddress);
        if (this.proxyChain != null) {
            for (int i = 0; i < this.proxyChain.length; i++) {
                hash = LangUtils.hashCode(hash, this.proxyChain[i]);
            }
        }
        hash = LangUtils.hashCode(hash, this.connected);
        hash = LangUtils.hashCode(hash, this.secure);
        hash = LangUtils.hashCode(hash, this.tunnelled);
        hash = LangUtils.hashCode(hash, this.layered);
        return hash;
    }

    /**
     * Obtains a description of the tracked route.
     *
     * @return  a human-readable representation of the tracked route
     */
    @Override
    public final String toString() {
        StringBuilder cab = new StringBuilder(50 + getHopCount()*30);

        cab.append("RouteTracker[");
        if (this.localAddress != null) {
            cab.append(this.localAddress);
            cab.append("->");
        }
        cab.append('{');
        if (this.connected)
            cab.append('c');
        if (this.tunnelled == TunnelType.TUNNELLED)
            cab.append('t');
        if (this.layered == LayerType.LAYERED)
            cab.append('l');
        if (this.secure)
            cab.append('s');
        cab.append("}->");
        if (this.proxyChain != null) {
            for (int i=0; i<this.proxyChain.length; i++) {
                cab.append(this.proxyChain[i]);
                cab.append("->");
            }
        }
        cab.append(this.targetHost);
        cab.append(']');

        return cab.toString();
    }


    // default implementation of clone() is sufficient
    @Override
    public Object clone() throws CloneNotSupportedException {
        return super.clone();
    }

}
