package com.navigation.esp32;

import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.util.UUID;

@SuppressLint("MissingPermission") // Permissions requested in MainActivity
public class BleClientManager {
    private static final String TAG = "BleClientManager";

    // Same UUIDs configured on the ESP32
    public static final UUID SERVICE_UUID = UUID.fromString("12345678-1234-5678-1234-56789abcdef0");
    public static final UUID CHAR_NAV_DATA_UUID = UUID.fromString("12345678-1234-5678-1234-56789abcdef1");

    private Context context;
    private BluetoothAdapter bluetoothAdapter;
    private BluetoothLeScanner bluetoothLeScanner;
    private BluetoothGatt bluetoothGatt;
    private Handler handler;

    public interface BleListener {
        void onConnectionStateChange(boolean connected);

        void onLog(String message);
    }

    private BleListener listener;

    public BleClientManager(Context context, BleListener listener) {
        this.context = context;
        this.listener = listener;
        this.handler = new Handler(Looper.getMainLooper());

        BluetoothManager bluetoothManager = (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        bluetoothAdapter = bluetoothManager.getAdapter();
    }

    public void startScan() {
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled())
            return;
        bluetoothLeScanner = bluetoothAdapter.getBluetoothLeScanner();
        listener.onLog("Starting BLE Scan...");
        bluetoothLeScanner.startScan(scanCallback);
    }

    private ScanCallback scanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            BluetoothDevice device = result.getDevice();
            if (device.getName() != null && device.getName().equals("ESP32_Nav")) {
                listener.onLog("Found ESP32! Connecting...");
                bluetoothLeScanner.stopScan(this);
                connectToDevice(device);
            }
        }
    };

    @SuppressLint("DiscouragedPrivateApi")
    private void refreshDeviceCache(BluetoothGatt gatt) {
        try {
            java.lang.reflect.Method localMethod = gatt.getClass().getMethod("refresh");
            if (localMethod != null) {
                boolean result = (Boolean) localMethod.invoke(gatt);
                handler.post(() -> listener.onLog("GATT Cache Cleared: " + result));
            }
        } catch (Exception localException) {
            handler.post(() -> listener.onLog("GATT Cache Clear Failed"));
        }
    }

    private void connectToDevice(BluetoothDevice device) {
        bluetoothGatt = device.connectGatt(context, false, gattCallback);
    }

    private BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                handler.post(() -> listener.onLog("Connected to GATT. Refreshing memory..."));
                refreshDeviceCache(gatt);
                handler.post(() -> listener.onConnectionStateChange(true));

                // Delay discovery slightly to allow the cache wipe to finalize
                handler.postDelayed(() -> gatt.discoverServices(), 600);
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                handler.post(() -> listener.onLog("Disconnected from GATT."));
                handler.post(() -> listener.onConnectionStateChange(false));
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                handler.post(() -> listener.onLog("Services discovered! Req MTU 512..."));
                // Print the specific services we found to debug exactly what the phone sees
                for (BluetoothGattService s : gatt.getServices()) {
                    String uuid = s.getUuid().toString();
                    if (uuid.startsWith("12345678")) {
                        handler.post(() -> listener.onLog("Found Target Service: " + uuid.substring(0, 8) + "..."));
                    }
                }
                gatt.requestMtu(512);
            } else {
                handler.post(() -> listener.onLog("Service discovery failed: " + status));
            }
        }

        @Override
        public void onMtuChanged(BluetoothGatt gatt, int mtu, int status) {
            super.onMtuChanged(gatt, mtu, status);
            handler.post(() -> listener.onLog("MTU agreed: " + mtu));
        }
    };

    public void sendNavUpdate(int maneuverId, String instruction, String street, String distance) {
        if (bluetoothGatt == null) {
            listener.onLog("sendNavUpdate: bluetoothGatt is null");
            return;
        }

        BluetoothGattService service = bluetoothGatt.getService(SERVICE_UUID);
        if (service == null) {
            listener.onLog("sendNavUpdate: Service not found");
            return;
        }

        BluetoothGattCharacteristic characteristic = service.getCharacteristic(CHAR_NAV_DATA_UUID);
        if (characteristic == null) {
            listener.onLog("sendNavUpdate: Characteristic not found");
            return;
        }

        // Simple CSV payload format matching ESP32 parser, now with prepended Maneuver
        // ID
        String payload = maneuverId + "," + instruction + "," + street + "," + distance;
        characteristic.setValue(payload.getBytes());
        // Fire-and-forget: do not wait for ESP32 acknowledgement to prevent Android BLE
        // stack locks
        characteristic.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE);

        boolean success = bluetoothGatt.writeCharacteristic(characteristic);
        handler.post(() -> listener.onLog("Sent Nav Payload | Success: " + success));
    }

    public void disconnect() {
        if (bluetoothGatt != null) {
            bluetoothGatt.disconnect();
            bluetoothGatt.close();
            bluetoothGatt = null;
        }
    }
}
