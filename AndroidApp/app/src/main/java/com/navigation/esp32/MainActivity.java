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
    private TextView tvLogs;
    private Button btnConnect;
    private Button btnMockData;
    private Button btnExportLog;
    private BleClientManager bleManager;
    private StringBuilder fullLogBuffer = new StringBuilder();

    private android.os.Handler mockHandler = new android.os.Handler(android.os.Looper.getMainLooper());
    private boolean isMocking = false;
    private int mockIndex = 0;
    private final String[] mockManeuvers = {
        "190 m \u00B7 Timotejv\u00E4gen 25",
        "300 m \u00B7 Rondell, 2:a avfarten",
        "50 m \u00B7 H\u00E5ll h\u00F6ger p\u00E5 E4",
        "400 m \u00B7 Sv\u00E4ng v\u00E4nster",
        "15 km \u00B7 Rakt fram p\u00E5 motorv\u00E4gen",
        "0 m \u00B7 Framme vid m\u00E5let"
    };

    private Runnable mockRunnable = new Runnable() {
        @Override
        public void run() {
            if (!isMocking) return;
            
            String mockTitle = mockManeuvers[mockIndex % mockManeuvers.length];
            String mockText = " ";
            String mockSubText = "Ankomst 14:00";
            String mockTime = "13:" + String.format("%02d", 55 + (mockIndex % 5));
            mockIndex++;

            appendLog("LIVE: " + mockTitle + " - " + mockText + " - " + mockSubText + " @ " + mockTime);

            if (bleManager != null) {
                bleManager.sendNavUpdate(mockTitle, mockText, mockSubText, mockTime);
            } else {
                Toast.makeText(MainActivity.this, "BLE Manager not initialized.", Toast.LENGTH_SHORT).show();
            }
            
            // Schedule next mock in 20 seconds
            mockHandler.postDelayed(this, 20000);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        tvStatus = findViewById(R.id.tvStatus);
        tvLogs = findViewById(R.id.tvLogs);
        btnConnect = findViewById(R.id.btnConnect);
        btnMockData = findViewById(R.id.btnMockData);
        btnExportLog = findViewById(R.id.btnExportLog);
        bleManager = new BleClientManager(this, this);

        checkPermissions();

        btnConnect.setOnClickListener(v -> {
            tvStatus.setText("Scanning for ESP32...");
            btnConnect.setEnabled(false);
            bleManager.startScan();
        });

        // Debug: Send mock payload to ESP32 without needing Google Maps
        btnMockData.setOnClickListener(v -> {
            if (isMocking) {
                isMocking = false;
                mockHandler.removeCallbacks(mockRunnable);
                btnMockData.setText("Start Mock");
            } else {
                isMocking = true;
                btnMockData.setText("Stop Mock");
                mockHandler.post(mockRunnable);
            }
        });

        btnExportLog.setOnClickListener(v -> {
            Intent sendIntent = new Intent();
            sendIntent.setAction(Intent.ACTION_SEND);
            sendIntent.putExtra(Intent.EXTRA_TEXT, fullLogBuffer.toString());
            sendIntent.setType("text/plain");
            Intent shareIntent = Intent.createChooser(sendIntent, "Export ESP32 Navigation Log");
            startActivity(shareIntent);
        });

        // Register receiver for Navigation Updates
        IntentFilter filter = new IntentFilter(NavNotificationService.ACTION_NAV_UPDATE);
        registerReceiver(navUpdateReceiver, filter);
    }

    private void appendLog(String message) {
        String logLine = new java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.getDefault()).format(new java.util.Date()) + " " + message + "\n";
        
        fullLogBuffer.append(logLine);
        if (fullLogBuffer.length() > 100000) {
            fullLogBuffer.delete(0, fullLogBuffer.length() - 50000);
        }

        String currentText = tvLogs.getText().toString();
        // Keep log from growing infinitely
        if (currentText.length() > 2000) {
            currentText = currentText.substring(currentText.length() - 1000);
        }
        tvLogs.setText(currentText + logLine);
    }

    private BroadcastReceiver navUpdateReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String title = intent.getStringExtra("title");
            String text = intent.getStringExtra("text");
            String subText = intent.getStringExtra("subText");
            String time = intent.getStringExtra("time");

            if (time == null) time = "00:00";
            if (title == null) title = "";
            if (text == null) text = "";
            if (subText == null) subText = "";

            appendLog("LIVE: " + title + " - " + text + " - " + subText + " @ " + time);

            if (bleManager != null) {
                bleManager.sendNavUpdate(title, text, subText, time);
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
    public void onLog(String message) {
        runOnUiThread(() -> appendLog("BLE: " + message));
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unregisterReceiver(navUpdateReceiver);
        if (bleManager != null)
            bleManager.disconnect();
    }

    private void checkPermissions() {
        // Request Bluetooth permissions
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, new String[] {
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
