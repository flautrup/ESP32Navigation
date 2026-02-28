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
        if (notification == null) return;

        Bundle extras = notification.extras;
        if (extras == null) return;

        // Example Google Maps Keys (These change often and require careful testing/parsing)
        // android.title is usually the "Turn right"
        // android.text is usually the Street name "Main St"
        // android.subText is usually the distance "500ft" or ETA

        String title = extras.getString(Notification.EXTRA_TITLE, "");
        String text = extras.getString(Notification.EXTRA_TEXT, "");
        String subText = extras.getString(Notification.EXTRA_SUB_TEXT, "");

        if (!title.isEmpty() || !text.isEmpty()) {
            Log.d(TAG, "Nav Update -> Title: " + title + " | Text: " + text + " | Sub: " + subText);

            // TODO: In a real implementation, you'd map "title" (e.g. "Turn right") to an enum/icon ID
            // For now we just broadcast the raw text to be sent via BLE
            Intent intent = new Intent(ACTION_NAV_UPDATE);
            intent.putExtra("instruction", title);
            intent.putExtra("street", text);
            intent.putExtra("distance", subText);
            sendBroadcast(intent);
        }
    }

    @Override
    public void onNotificationRemoved(StatusBarNotification sbn) {
        if (sbn.getPackageName().equals(GOOGLE_MAPS_PACKAGE)) {
            Log.d(TAG, "Navigation Ended.");
            Intent intent = new Intent(ACTION_NAV_UPDATE);
            intent.putExtra("instruction", "Arrived or Ended");
            intent.putExtra("street", "");
            intent.putExtra("distance", "");
            sendBroadcast(intent);
        }
    }
}
