package io.openrct2;

import android.annotation.SuppressLint;
import android.content.Intent;
import android.icu.util.Currency;
import android.icu.util.LocaleData;
import android.icu.util.ULocale;
import android.net.Uri;
import android.os.Build;
import android.util.Log;
import android.view.View;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.util.Locale;

public class GameActivity extends SDLActivity {

    public float getDefaultScale() {
        return getResources().getDisplayMetrics().density;
    }

    @SuppressLint("ObsoleteSdkInt")
    private Locale getDeviceLocale() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            return getResources().getConfiguration().getLocales().get(0);
        } else {
            return getResources().getConfiguration().locale;
        }
    }
    public String getDefaultLocale(String[] supportedTags) {
        Locale deviceLocale = getDeviceLocale();

        for (String supportedTag : supportedTags) {
            if (supportedTag.isEmpty()) continue;
            String[] splits = supportedTag.split("-");
            String language = splits[0];
            String country = splits[1];
            if (deviceLocale.getLanguage().equals(language) && deviceLocale.getCountry().equals(country)) {
                return supportedTag;
            }
        }

        Locale canadaEn = Locale.CANADA;
        if (canadaEn.getLanguage().equals(deviceLocale.getLanguage()) && canadaEn.getCountry().equals(deviceLocale.getCountry())) {
            return "en-US";
        }

        for (String supportedTag : supportedTags) {
            if (supportedTag.isEmpty()) continue;
            String[] splits = supportedTag.split("-");
            String language = splits[0];
            if (deviceLocale.getLanguage().equals(language)) {
                return supportedTag;
            }
        }
        return "en-UK";
    }

    public String getLocaleCurrency() {
        Locale deviceLocale = getDeviceLocale();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            return Currency.getInstance(deviceLocale).getCurrencyCode();
        } else {
            return java.util.Currency.getInstance(deviceLocale).getCurrencyCode();
        }
    }

    public boolean isImperialLocaleMeasurementFormat() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return LocaleData.getMeasurementSystem(ULocale.forLocale(getResources().getConfiguration().getLocales().get(0))) == LocaleData.MeasurementSystem.US;
        }
        Locale deviceLocale = getDeviceLocale();
        String localeCountry = deviceLocale.getCountry();
        return localeCountry.equals(Locale.US.getCountry()) || localeCountry.equals(new Locale("xx", "LR").getCountry()) || localeCountry.equals(new Locale("xx", "MM").getCountry());
    }

    @Override
    protected String[] getLibraries() {
        return new String[]{
                "c++_shared",
                "bz2",
                "freetype",
                "z",
                "png16",
                "SDL2",
                "icudata",
                "icuuc",
                "icui18n",

                "openrct2",
                "openrct2-ui"
        };
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        // Set app to fullscreen mode
        if (hasFocus) {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            );
        }
    }

    protected String[] getArguments() {
        if (getIntent().hasExtra("commandLineArgs")) {
            return getIntent().getStringArrayExtra("commandLineArgs");
        }
        return new String[0];
    }

    public void launchURL(String url) {
        try {
            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            startActivity(intent);
        } catch (Exception e) {
            Log.e("OpenRCT2", "Failed to open URL: " + url, e);
        }
    }

    public void launchFolder(String path) {
        try {
            File file = new File(path);
            Intent intent = new Intent(Intent.ACTION_VIEW);
            Uri uri = Uri.fromFile(file);
            intent.setDataAndType(uri, "resource/folder");
            if (intent.resolveActivity(getPackageManager()) != null) {
                startActivity(intent);
            } else {
                Log.w("OpenRCT2", "No activity found to open folder: " + path);
            }
        } catch (Exception e) {
            Log.e("OpenRCT2", "Failed to open folder: " + path, e);
        }
    }
}
