package com.navigation.esp32;

import android.content.Intent;
import android.os.Bundle;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;
import android.util.Log;
import android.app.Notification;

public class NavNotificationService extends NotificationListenerService {

    private static final String TAG = "NavNotificationService";
    private static final String GOOGLE_MAPS_PACKAGE = "com.google.android.apps.maps";

    // Intent Action to broadcast parsed data to our BLE service or Activity
    public static final String ACTION_NAV_UPDATE = "com.navigation.esp32.NAV_UPDATE";

    @Override
    public void onNotificationPosted(StatusBarNotification sbn) {
        if (!sbn.getPackageName().equals(GOOGLE_MAPS_PACKAGE)) {
            return;
        }

        Notification notification = sbn.getNotification();
        if (notification == null)
            return;

        Bundle extras = notification.extras;
        if (extras == null)
            return;

        // Example Google Maps Keys (These change often and require careful
        // testing/parsing)
        // android.title is usually the "Turn right"
        // android.text is usually the Street name "Main St"
        // android.subText is usually the distance "500ft" or ETA

        // Dump all keys to logcat for deeper debugging if text is still missing
        for (String key : extras.keySet()) {
            Object value = extras.get(key);
            Log.v(TAG, "EXTRA_DUMP -> " + key + ": " + value);
        }

        CharSequence titleCs = extras.getCharSequence(Notification.EXTRA_TITLE);
        CharSequence textCs = extras.getCharSequence(Notification.EXTRA_TEXT);
        CharSequence subTextCs = extras.getCharSequence(Notification.EXTRA_SUB_TEXT);

        String title = titleCs != null ? titleCs.toString() : "";
        String text = textCs != null ? textCs.toString() : "";
        String subText = subTextCs != null ? subTextCs.toString() : "";

        String currentTime = new java.text.SimpleDateFormat("HH:mm", java.util.Locale.getDefault()).format(new java.util.Date());

        if (!title.isEmpty() || !text.isEmpty() || !subText.isEmpty()) {
            Log.d(TAG, "Nav Update -> Title: " + title + " | Text: " + text + " | SubText: " + subText + " | Time: " + currentTime);

            Intent intent = new Intent(ACTION_NAV_UPDATE);
            intent.putExtra("title", title);
            intent.putExtra("text", text);
            intent.putExtra("subText", subText);
            intent.putExtra("time", currentTime);
            sendBroadcast(intent);
        }
    }

    @Override
    public void onNotificationRemoved(StatusBarNotification sbn) {
        if (sbn.getPackageName().equals(GOOGLE_MAPS_PACKAGE)) {
            Log.d(TAG, "Navigation Ended.");
            Intent intent = new Intent(ACTION_NAV_UPDATE);
            intent.putExtra("title", "Ended");
            intent.putExtra("text", "");
            intent.putExtra("subText", "");
            intent.putExtra("time", "");
            sendBroadcast(intent);
        }
    }
}
