package com.navigation.esp32;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.provider.Settings;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;
import android.Manifest;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

public class MainActivity extends AppCompatActivity implements BleClientManager.BleListener {

    private TextView tvStatus;
    private Button btnConnect;
    private BleClientManager bleManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        tvStatus = findViewById(R.id.tvStatus);
        btnConnect = findViewById(R.id.btnConnect);
        bleManager = new BleClientManager(this, this);

        checkPermissions();

        btnConnect.setOnClickListener(v -> {
            tvStatus.setText("Scanning for ESP32...");
            btnConnect.setEnabled(false);
            bleManager.startScan();
        });

        // Register receiver for Navigation Updates
        IntentFilter filter = new IntentFilter(NavNotificationService.ACTION_NAV_UPDATE);
        registerReceiver(navUpdateReceiver, filter);
    }

    private BroadcastReceiver navUpdateReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String instruction = intent.getStringExtra("instruction");
            String street = intent.getStringExtra("street");
            String distance = intent.getStringExtra("distance");
            
            if (bleManager != null) {
                bleManager.sendNavUpdate(instruction, street, distance);
            }
        }
    };

    @Override
    public void onConnectionStateChange(boolean connected) {
        runOnUiThread(() -> {
            if (connected) {
                tvStatus.setText(R.string.status_connected);
                btnConnect.setEnabled(false);
                btnConnect.setText("Connected");
            } else {
                tvStatus.setText(R.string.status_disconnected);
                btnConnect.setEnabled(true);
                btnConnect.setText(R.string.scan_button);
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unregisterReceiver(navUpdateReceiver);
        if (bleManager != null) bleManager.disconnect();
    }

    private void checkPermissions() {
        // Request Bluetooth permissions
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, new String[]{
                    Manifest.permission.BLUETOOTH_SCAN,
                    Manifest.permission.BLUETOOTH_CONNECT,
                    Manifest.permission.ACCESS_FINE_LOCATION
            }, 100);
        }

        // Check if we have notification listener access
        String enabledListeners = Settings.Secure.getString(getContentResolver(), "enabled_notification_listeners");
        if (enabledListeners == null || !enabledListeners.contains(getPackageName())) {
            Toast.makeText(this, R.string.permission_required, Toast.LENGTH_LONG).show();
            Intent intent = new Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS);
            startActivity(intent);
        }
    }
}
