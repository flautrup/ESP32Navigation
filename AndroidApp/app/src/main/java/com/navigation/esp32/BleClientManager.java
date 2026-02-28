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
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled()) return;
        bluetoothLeScanner = bluetoothAdapter.getBluetoothLeScanner();
        Log.d(TAG, "Starting BLE Scan...");
        bluetoothLeScanner.startScan(scanCallback);
    }

    private ScanCallback scanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            BluetoothDevice device = result.getDevice();
            if (device.getName() != null && device.getName().equals("ESP32_Nav")) {
                Log.d(TAG, "Found ESP32_Nav! Connecting...");
                bluetoothLeScanner.stopScan(this);
                connectToDevice(device);
            }
        }
    };

    private void connectToDevice(BluetoothDevice device) {
        bluetoothGatt = device.connectGatt(context, false, gattCallback);
    }

    private BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.d(TAG, "Connected to GATT server.");
                handler.post(() -> listener.onConnectionStateChange(true));
                gatt.discoverServices();
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.d(TAG, "Disconnected from GATT server.");
                handler.post(() -> listener.onConnectionStateChange(false));
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(TAG, "Services discovered.");
            }
        }
    };

    public void sendNavUpdate(String instruction, String street, String distance) {
        if (bluetoothGatt == null) return;

        BluetoothGattService service = bluetoothGatt.getService(SERVICE_UUID);
        if (service == null) return;

        BluetoothGattCharacteristic characteristic = service.getCharacteristic(CHAR_NAV_DATA_UUID);
        if (characteristic == null) return;

        // Simple CSV payload format matching ESP32 parser
        String payload = instruction + "," + street + "," + distance;
        characteristic.setValue(payload.getBytes());
        bluetoothGatt.writeCharacteristic(characteristic);
        Log.d(TAG, "Sent Nav Payload: " + payload);
    }

    public void disconnect() {
        if (bluetoothGatt != null) {
            bluetoothGatt.disconnect();
            bluetoothGatt.close();
            bluetoothGatt = null;
        }
    }
}
