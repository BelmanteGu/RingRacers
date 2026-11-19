package org.kartkrew.ringracers;

import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.RelativeLayout;

import org.libsdl.app.SDLActivity;

import java.io.File;

public final class GameActivity extends SDLActivity {
    private final Handler handler = new Handler(Looper.getMainLooper());
    private static final boolean USE_JAVA_OVERLAY = false;
    private TouchControlsView touchControls;

    @Override
    protected String[] getLibraries() {
        return new String[]{"main"};
    }

    @Override
    protected String[] getArguments() {
        File gameDirectory = new File(getFilesDir(), "game");

        // Use external storage (internal shared storage /sdcard) if accessible,
        // so users can access /sdcard/RingRacers and /sdcard/RingRacers/addons in their file manager.
        File storageRoot = Environment.getExternalStorageDirectory();
        if (storageRoot == null || !storageRoot.canWrite()) {
            storageRoot = getExternalFilesDir(null);
        }
        if (storageRoot == null) {
            storageRoot = getFilesDir();
        }

        File ringRacersDir = new File(storageRoot, "RingRacers");
        File addonsDir = new File(ringRacersDir, "addons");
        ringRacersDir.mkdirs();
        addonsDir.mkdirs();

        int[] safe = computeSafeArea();
        return new String[]{
            "-waddir", gameDirectory.getAbsolutePath(),
            "-home", storageRoot.getAbsolutePath(),
            "-safearea", String.valueOf(safe[0]), String.valueOf(safe[1]),
                String.valueOf(safe[2]), String.valueOf(safe[3])
        };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        enableImmersiveMode();

        // The on-screen controls are now drawn by the engine itself
        // (src/sdl/i_touch.cpp) with the game's button graphics and live in
        // Options > Touch & Display. The old Java overlay is kept but hidden.
        if (USE_JAVA_OVERLAY && !mBrokenLibraries && mLayout != null) {
            touchControls = new TouchControlsView(this);
            RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.MATCH_PARENT,
                RelativeLayout.LayoutParams.MATCH_PARENT
            );
            mLayout.addView(touchControls, params);
        }
    }

    @Override
    protected void onPause() {
        if (touchControls != null) {
            touchControls.releaseAll();
        }
        super.onPause();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            enableImmersiveMode();
        } else if (touchControls != null) {
            touchControls.releaseAll();
        }
    }

    @Override
    public void onBackPressed() {
        if (mBrokenLibraries) {
            super.onBackPressed();
            return;
        }
        if (touchControls != null && touchControls.isEditMode()) {
            touchControls.exitEditMode(true);
            return;
        }
        if (SDLActivity.isScreenKeyboardShown()) {
            hideKeyboard();
            return;
        }
        SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_ESCAPE);
        handler.postDelayed(() -> SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_ESCAPE), 40L);
    }

    public void toggleKeyboard() {
        handler.post(() -> {
            if (SDLActivity.isScreenKeyboardShown()) {
                hideKeyboard();
            } else {
                showKeyboard();
            }
        });
    }

    public void showKeyboard() {
        handler.post(() -> SDLActivity.showTextInput(0, 0, 0, 0));
    }

    public void hideKeyboard() {
        handler.post(() -> SDLActivity.sendMessage(3, 0));
    }

    private void enableImmersiveMode() {
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        );
    }

    /**
     * The phone's safe area in landscape (camera cutout + rounded corners),
     * as fractions x10000 of the screen: left, top, right, bottom. The engine
     * keeps the HUD inside it (Options > Touch & Display > HUD Safe Area).
     * Left/right are made symmetric because sensorLandscape can flip sides.
     */
    private int[] computeSafeArea() {
        float sideFallback = 0.03f, topFallback = 0.0f;
        try {
            android.util.DisplayMetrics dm = new android.util.DisplayMetrics();
            getWindowManager().getDefaultDisplay().getRealMetrics(dm);
            int w = Math.max(dm.widthPixels, dm.heightPixels);
            int h = Math.min(dm.widthPixels, dm.heightPixels);
            int side = 0, vert = 0, corner = 0;

            android.view.WindowInsets wi = getWindow().getDecorView().getRootWindowInsets();
            android.view.Display display = getWindowManager().getDefaultDisplay();
            android.view.DisplayCutout cutout = (wi != null) ? wi.getDisplayCutout() : null;
            if (cutout == null && android.os.Build.VERSION.SDK_INT >= 29) {
                cutout = display.getCutout();
            }
            if (cutout != null) {
                // Cutouts sit on a short edge: in landscape that's left/right.
                int l = cutout.getSafeInsetLeft(), r = cutout.getSafeInsetRight();
                int t = cutout.getSafeInsetTop(), b = cutout.getSafeInsetBottom();
                boolean portraitInsets = (dm.heightPixels > dm.widthPixels);
                side = portraitInsets ? Math.max(t, b) : Math.max(l, r);
                vert = portraitInsets ? Math.max(l, r) : Math.max(t, b);
            }
            if (android.os.Build.VERSION.SDK_INT >= 31) {
                for (int pos = 0; pos < 4; pos++) {
                    android.view.RoundedCorner rc = display.getRoundedCorner(pos);
                    if (rc != null) corner = Math.max(corner, rc.getRadius());
                }
            }
            // A box hugging both edges clears a corner of radius r once it is
            // r*(1-1/sqrt2) ~ 0.3r in on each axis.
            int cornerPad = (int) (corner * 0.3f);
            side = Math.max(side, cornerPad);
            vert = Math.max(vert, cornerPad);
            if (side == 0) side = (int) (w * sideFallback);
            int fs = Math.round(side * 10000f / w), fv = Math.round(vert * 10000f / h);
            return new int[]{fs, fv, fs, fv};
        } catch (Throwable t) {
            int fs = Math.round(sideFallback * 10000f), fv = Math.round(topFallback * 10000f);
            return new int[]{fs, fv, fs, fv};
        }
    }
}
