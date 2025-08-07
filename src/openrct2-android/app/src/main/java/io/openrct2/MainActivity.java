package io.openrct2;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.graphics.Point;
import android.graphics.PointF;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.appcompat.app.AppCompatActivity;

import android.os.Environment;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.Surface;

import org.apache.commons.io.IOUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class MainActivity extends AppCompatActivity {

    public static final String TAG = "OpenRCT2";
    private boolean assetsCopied = false;

    @Override
    public void onRequestPermissionsResult(
        int requestCode,
        @NonNull String[] permissions,
        @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        if (hasRequiredPermissions()) {
            startGame();
        }
    }

    private String[] getSupportedAbis() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return Build.SUPPORTED_ABIS;
        }

        if (!TextUtils.isEmpty(Build.CPU_ABI2)) {
            return new String[]{Build.CPU_ABI, Build.CPU_ABI2};
        }

        return new String[]{Build.CPU_ABI};
    }

    private PointF getResolutionDips() {
        PointF out = new PointF();
        Point pixelSize = new Point();

        Display display = getWindowManager().getDefaultDisplay();

        DisplayMetrics metrics = new DisplayMetrics();
        display.getMetrics(metrics);
        display.getRealSize(pixelSize);
        display.getRealMetrics(metrics);

        int rotation = display.getRotation();
        if (rotation == Surface.ROTATION_90 || rotation == Surface.ROTATION_270) {
            pixelSize = new Point(pixelSize.y, pixelSize.x);
        }

        out.x = ((float) pixelSize.x) / metrics.density;
        out.y = ((float) pixelSize.y) / metrics.density;

        return out;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        String[] supportedAbis = getSupportedAbis();
        PointF resolution = getResolutionDips();
        DisplayMetrics displayMetrics = getResources().getDisplayMetrics();
    }

    @Override
    protected void onStart() {
        super.onStart();

        if (!hasRequiredPermissions()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                /*
                TODO - This is a temporary solution
                Ideally, OpenRCT2 assets should be shipped in the apk and we should ask
                the user for their rct2 assets directory on boot (similar to windows).
                Save data should be stored under the "external data directory",
                which you can get via `getExternalFilesDir(null).getAbsolutePath()`
                 */
                Intent intent = new Intent(android.provider.Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                intent.addCategory("android.intent.category.DEFAULT");
                intent.setData(Uri.fromParts("package", getPackageName(), null));
                startActivity(intent);
            } else {
                ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE}, 1);
            }
        } else {
            startGame();
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 0) {
            if (!hasRequiredPermissions()) {
                Log.d(TAG, "User denied storage permission!");
            } else {
                startGame();
            }
        }
    }

    private boolean hasRequiredPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return Environment.isExternalStorageManager();
        } else {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_DENIED) {
                return false;
            }

            if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_DENIED) {
                return false;
            }

            return true;
        }
    }

    private void startGame() {
        // Phase 2: Check if we have embedded assets and skip file copying
        if (hasEmbeddedAssets()) {
            Log.i(TAG, "Phase 2: Found embedded assets - skipping file copying");
            assetsCopied = true; // Mark as "copied" to satisfy legacy checks
        } else {
            Log.w(TAG, "Phase 2: No embedded assets found, falling back to file copying");
            copyAssets(); // TODO Don't copy/enumerate assets on every startup
        }

        Intent intent = new Intent(this, GameActivity.class);
        if (getIntent().hasExtra("commandLineArgs")) {
            intent.putExtra("commandLineArgs", getIntent().getStringArrayExtra("commandLineArgs"));
        }
        startActivity(intent);
        finish();
    }

    /**
     * Phase 2: Check if we have embedded assets by looking for key files
     * @return true if critical OpenRCT2 assets are embedded in APK
     */
    private boolean hasEmbeddedAssets() {
        try {
            AssetManager assets = getAssets();

            // Check for critical OpenRCT2 files that should be embedded
            String[] criticalAssets = {
                "openrct2/g2.dat",
                "openrct2/fonts.dat",
                "openrct2/tracks.dat",
                "openrct2/language/en-GB.txt"
            };

            for (String assetPath : criticalAssets) {
                try {
                    InputStream stream = assets.open(assetPath);
                    stream.close();
                    Log.d(TAG, "Phase 2: Found embedded asset: " + assetPath);
                } catch (IOException e) {
                    Log.w(TAG, "Phase 2: Missing embedded asset: " + assetPath);
                    return false;
                }
            }

            Log.i(TAG, "Phase 2: All critical assets found embedded in APK");
            return true;

        } catch (Exception e) {
            Log.e(TAG, "Phase 2: Error checking for embedded assets", e);
            return false;
        }
    }    // TODO Don't copy/enumerate assets on every startup
    // When building, ensure OpenRCT2 assets are inside their own directory within the APK assets,
    // so that we do not attempt to copy files out of the standard Android asset folders - webkit, etc.
    private void copyAssets() {
        File dataDir = new File(Environment.getExternalStorageDirectory().toString()
            + File.separator + "openrct2" + File.separator);

        try {
            copyAsset(getAssets(), "openrct2", dataDir, "");
        } catch (IOException e) {
            Log.e(TAG, "Error extracting files", e);
            return;
        }

        assetsCopied = true;
    }

    // srcPath cannot be the empty string
    private void copyAsset(AssetManager assets, String srcPath, File dataDir, String destPath) throws IOException {
        String[] list = assets.list(srcPath);

        if (list.length == 0) {
            InputStream input = assets.open(srcPath);
            File extractedFile = new File(dataDir, destPath);
            File parentFile = extractedFile.getParentFile();
            if (!parentFile.exists()) {
                boolean success = parentFile.mkdirs();
                if (!success) {
                    Log.d(TAG, String.format("Error creating folder '%s'", parentFile));
                }
            }
            FileOutputStream output = new FileOutputStream(extractedFile);
            IOUtils.copyLarge(input, output);
            output.close();
            input.close();
            return;
        }

        for (String fileName : list) {
            // This ternary expression makes sure that this string does not begin with a slash
            String destination = destPath + (destPath.isEmpty() ? "" : File.separator) + fileName;
            copyAsset(assets, srcPath + File.separator + fileName, dataDir, destination);
        }
    }

}
