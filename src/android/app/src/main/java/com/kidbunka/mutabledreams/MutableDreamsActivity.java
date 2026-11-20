// SPDX-License-Identifier: GPL-3.0-only
// Mutable Dreams for Android, by Kid Bunka: SDL's activity around the Brick port's core (libmain.so).
package com.kidbunka.mutabledreams;

import android.os.Bundle;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;

public class MutableDreamsActivity extends SDLActivity {
    private static final String TAG = "MutableDreams";

    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL2", "main"};
    }

    /** The package's files (worlds, archive catalogue, texts) into the app's folder on the device's storage,
     *  Android/data/com.kidbunka.mutabledreams/files, where the core looks for them beside save/. A file that is
     *  already there stays as it is (your own worlds.txt, your samples), except the texts, which follow the app. */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        File base = getExternalFilesDir(null);
        if (base != null) {
            try (BufferedReader list = new BufferedReader(new InputStreamReader(getAssets().open("md/files.txt")))) {
                String rel;
                while ((rel = list.readLine()) != null) {
                    rel = rel.trim();
                    if (rel.isEmpty())
                        continue;
                    File to = new File(base, rel);
                    boolean text = rel.endsWith("README.txt") || rel.startsWith("LICENSE") || rel.equals("SOURCE.txt")
                            || rel.equals("ANDROID.txt") || rel.startsWith("lang/");   // (1.0: the language table follows the app)
                    if (to.exists() && !text)
                        continue;
                    File dir = to.getParentFile();
                    if (dir != null && !dir.isDirectory() && !dir.mkdirs())
                        continue;
                    try (InputStream in = getAssets().open("md/" + rel); OutputStream out = new FileOutputStream(to)) {
                        byte[] buf = new byte[16384];
                        int n;
                        while ((n = in.read(buf)) > 0)
                            out.write(buf, 0, n);
                    }
                }
                new File(base, "save").mkdirs();
                new File(base, "samples/voices").mkdirs();
            } catch (Exception e) {
                Log.w(TAG, "could not put the package's files in place", e);
            }
        }
        super.onCreate(savedInstanceState);
    }
}
