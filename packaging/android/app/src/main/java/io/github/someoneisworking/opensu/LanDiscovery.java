package io.github.someoneisworking.opensu;

import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.net.wifi.WifiManager;
import android.util.Log;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.LinkedHashMap;
import java.util.Map;

/** This openSU announced as `_opensu._tcp` through NsdManager, and the other openSUs it finds. */
final class LanDiscovery {
    private static final String TYPE = "_opensu._tcp";
    private static final String TAG = "openSU";

    private final NsdManager nsd;
    private final WifiManager.MulticastLock multicast;
    private final String selfId;
    private final NsdServiceInfo self;
    private String announced;
    /** Each resolved service by its name: "id TAB name TAB address TAB port". */
    private final Map<String, String> found = new LinkedHashMap<>();
    /** Services waiting to be resolved; NsdManager resolves one at a time. */
    private final ArrayDeque<NsdServiceInfo> waiting = new ArrayDeque<>();
    private boolean resolving;
    private boolean stopped;

    private final NsdManager.RegistrationListener registration = new NsdManager.RegistrationListener() {
        @Override
        public void onServiceRegistered(NsdServiceInfo info) {
            synchronized (LanDiscovery.this) {
                announced = info.getServiceName();
            }
            Log.i(TAG, "lan: announced as " + info.getServiceName());
        }

        @Override
        public void onRegistrationFailed(NsdServiceInfo info, int error) {
            Log.w(TAG, "lan: the announcement failed, NSD error " + error);
        }

        @Override
        public void onServiceUnregistered(NsdServiceInfo info) {
        }

        @Override
        public void onUnregistrationFailed(NsdServiceInfo info, int error) {
            Log.w(TAG, "lan: the announcement could not be withdrawn, NSD error " + error);
        }
    };

    private final NsdManager.DiscoveryListener discovery = new NsdManager.DiscoveryListener() {
        @Override
        public void onDiscoveryStarted(String type) {
        }

        @Override
        public void onDiscoveryStopped(String type) {
        }

        @Override
        public void onStartDiscoveryFailed(String type, int error) {
            Log.w(TAG, "lan: looking for openSUs failed, NSD error " + error);
        }

        @Override
        public void onStopDiscoveryFailed(String type, int error) {
        }

        @Override
        public void onServiceFound(NsdServiceInfo info) {
            synchronized (LanDiscovery.this) {
                waiting.add(info);
            }
            resolveNext();
        }

        @Override
        public void onServiceLost(NsdServiceInfo info) {
            synchronized (LanDiscovery.this) {
                found.remove(info.getServiceName());
            }
        }
    };

    LanDiscovery(Context context, String id, String name, int port) {
        nsd = (NsdManager) context.getSystemService(Context.NSD_SERVICE);
        WifiManager wifi = (WifiManager) context.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        multicast = wifi != null ? wifi.createMulticastLock("opensu-lan") : null;
        selfId = id;
        announced = name;
        self = new NsdServiceInfo();
        self.setServiceName(name);
        self.setServiceType(TYPE);
        self.setPort(port);
        self.setAttribute("id", id);
        self.setAttribute("name", name);
        self.setAttribute("v", "2");
    }

    /** Announces and starts looking: null, else why it cannot. */
    String start() {
        if (nsd == null) {
            return "this device has no network service discovery";
        }
        if (multicast != null) {
            // Without it the Wi-Fi driver may drop the multicast answers mDNS lives on.
            multicast.setReferenceCounted(false);
            multicast.acquire();
        }
        try {
            nsd.registerService(self, NsdManager.PROTOCOL_DNS_SD, registration);
            nsd.discoverServices(TYPE, NsdManager.PROTOCOL_DNS_SD, discovery);
        } catch (IllegalArgumentException | SecurityException refused) {
            stop();
            return refused.toString();
        }
        return null;
    }

    void stop() {
        synchronized (this) {
            stopped = true;
            waiting.clear();
            found.clear();
        }
        try {
            nsd.stopServiceDiscovery(discovery);
        } catch (IllegalArgumentException notStarted) {
            // It never started.
        }
        try {
            nsd.unregisterService(registration);
        } catch (IllegalArgumentException notRegistered) {
            // It was never registered.
        }
        if (multicast != null && multicast.isHeld()) {
            multicast.release();
        }
    }

    synchronized String[] sightings() {
        return found.values().toArray(new String[0]);
    }

    synchronized String announcedName() {
        return announced;
    }

    private void resolveNext() {
        NsdServiceInfo next;
        synchronized (this) {
            if (resolving || stopped || waiting.isEmpty()) {
                return;
            }
            resolving = true;
            next = waiting.poll();
        }
        nsd.resolveService(next, new NsdManager.ResolveListener() {
            @Override
            public void onResolveFailed(NsdServiceInfo info, int error) {
                Log.i(TAG, "lan: " + info.getServiceName() + " did not resolve, NSD error " + error);
                done();
            }

            @Override
            public void onServiceResolved(NsdServiceInfo info) {
                keep(info);
                done();
            }
        });
    }

    private void done() {
        synchronized (this) {
            resolving = false;
        }
        resolveNext();
    }

    private static String text(Map<String, byte[]> attributes, String key) {
        byte[] value = attributes.get(key);
        return value != null ? new String(value, StandardCharsets.UTF_8) : "";
    }

    private void keep(NsdServiceInfo info) {
        InetAddress host = info.getHost();
        Map<String, byte[]> attributes = info.getAttributes();
        String id = text(attributes, "id");
        String name = text(attributes, "name");
        // Only IPv4 resolutions are kept, as the desktop's backend does.
        if (!(host instanceof Inet4Address) || id.isEmpty() || id.equals(selfId)) {
            return;
        }
        String row = id + "\t" + (name.isEmpty() ? info.getServiceName() : name) + "\t"
                + host.getHostAddress() + "\t" + info.getPort();
        synchronized (this) {
            if (!stopped) {
                found.put(info.getServiceName(), row);
            }
        }
    }
}
