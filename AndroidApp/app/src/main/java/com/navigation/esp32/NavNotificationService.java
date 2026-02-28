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

        String title = extras.getString(Notification.EXTRA_TITLE, "");
        String text = extras.getString(Notification.EXTRA_TEXT, "");
        String subText = extras.getString(Notification.EXTRA_SUB_TEXT, "");

        if (!title.isEmpty() || !text.isEmpty()) {
            Log.d(TAG, "Nav Update -> Title: " + title + " | Text: " + text + " | Sub: " + subText);

            int maneuverId = parseManeuverId(title);

            Intent intent = new Intent(ACTION_NAV_UPDATE);
            intent.putExtra("maneuver_id", maneuverId);
            intent.putExtra("instruction", title);
            intent.putExtra("street", text);
            intent.putExtra("distance", subText);
            sendBroadcast(intent);
        }
    }

    private int parseManeuverId(String instruction) {
        if (instruction == null || instruction.isEmpty())
            return 0; // Default / Straight
        String lower = instruction.toLowerCase().trim();

        if (lower.contains("u-turn"))
            return 5;
        if (lower.contains("roundabout"))
            return 6;
        if (lower.contains("keep left") || lower.contains("bear left"))
            return 3;
        if (lower.contains("keep right") || lower.contains("bear right"))
            return 4;
        if (lower.contains("left"))
            return 1;
        if (lower.contains("right"))
            return 2;
        if (lower.contains("arrive") || lower.contains("destination"))
            return 7;

        return 0; // Default straight arrow
    }

    @Override
    public void onNotificationRemoved(StatusBarNotification sbn) {
        if (sbn.getPackageName().equals(GOOGLE_MAPS_PACKAGE)) {
            Log.d(TAG, "Navigation Ended.");
            Intent intent = new Intent(ACTION_NAV_UPDATE);
            intent.putExtra("maneuver_id", 7); // Arrived/Ended flag
            intent.putExtra("instruction", "Arrived or Ended");
            intent.putExtra("street", "");
            intent.putExtra("distance", "");
            sendBroadcast(intent);
        }
    }
}
