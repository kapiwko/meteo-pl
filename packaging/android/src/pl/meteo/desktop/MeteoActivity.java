package pl.meteo.desktop;

import android.Manifest;
import android.content.ComponentName;
import android.content.pm.PackageManager;
import android.os.Bundle;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicInteger;
import org.qtproject.qt.android.bindings.QtActivity;

public final class MeteoActivity extends QtActivity {
    private static final int LOCATION_PERMISSION_REQUEST = 0x4d45;
    private final ArrayList<AtomicInteger> locationRequests = new ArrayList<>();
    private boolean locationDenied = false;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        // Qt skips native library loading when the launch intent names an
        // alias. Keep old launcher shortcuts working with the new Activity.
        getIntent().setComponent(new ComponentName(this, MeteoActivity.class));
        super.onCreate(savedInstanceState);
    }

    // Called only on Android's UI thread. Atomic results can be read from Qt's
    // GUI thread without blocking Android's accessibility callbacks.
    public AtomicInteger requestMeteoLocationPermission() {
        AtomicInteger result = new AtomicInteger(-1);
        if (checkSelfPermission(Manifest.permission.ACCESS_COARSE_LOCATION)
                == PackageManager.PERMISSION_GRANTED) {
            result.set(1);
        } else if (locationDenied) {
            result.set(0);
        } else {
            locationRequests.add(result);
            if (locationRequests.size() == 1) {
                requestPermissions(new String[] { Manifest.permission.ACCESS_COARSE_LOCATION },
                        LOCATION_PERMISSION_REQUEST);
            }
        }
        return result;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode != LOCATION_PERMISSION_REQUEST) {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
            return;
        }
        boolean granted = grantResults.length > 0
                && grantResults[0] == PackageManager.PERMISSION_GRANTED;
        locationDenied = !granted;
        for (AtomicInteger result : locationRequests) result.set(granted ? 1 : 0);
        locationRequests.clear();
    }
}
