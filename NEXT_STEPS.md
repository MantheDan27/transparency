# Device Monitor - Complete Setup Guide

## ✅ What Has Been Completed

### 1. Desktop Application (Electron)
- **Location**: `device-monitor-desktop/`
- **Files Created**:
  - `main.js` - Electron backend with system scanning
  - `src/index.html` - Professional UI
  - `src/renderer.js` - Frontend logic
  - `src/styles.css` - Black and blue theme
  - `package.json` - Dependencies and build scripts
  - `system-scanner.js` - Advanced threat detection module

### 2. Documentation
- **CONTRIBUTING.md** - Developer guidelines
- **RELEASES.md** - Version management
- **ANDROID_STUDIO_INTEGRATION.md** - Cross-platform guide
- **README.md** - Project overview
- **LICENSE** - MIT License

### 3. CI/CD Pipeline
- **Location**: `.github/workflows/build.yml`
- **Features**:
  - Automatic builds on push
  - Builds for Windows, macOS, Linux
  - Automatic release creation on tags
  - Runs on: Ubuntu, Windows, macOS

## 🚀 Next: Connect to Android Studio

### Quick Start (5 Steps)

#### Step 1: Install Android Studio
```bash
# Download from: https://developer.android.com/studio
# Install Java Development Kit (JDK 11+)
```

#### Step 2: Clone in Android Studio
```
File → New → Project from Version Control
Select Git → Paste: https://github.com/MantheD27/transparency.git
Click Clone
```

#### Step 3: Create Android Module
```
Right-click project root → New → Module
Select Android App
Module name: device-monitor-android
Package: com.devicemonitor.android
Minimum SDK: API 26
Finish
```

#### Step 4: Update build.gradle

**File**: `device-monitor-android/build.gradle`

```gradle
android {
    compileSdk 33
    defaultConfig {
        minSdk 26
        targetSdk 33
        versionCode 1
        versionName "1.0.0"
    }
}

dependencies {
    implementation 'androidx.appcompat:appcompat:1.5.0'
    implementation 'com.squareup.okhttp3:okhttp:4.10.0'
    implementation 'com.google.code.gson:gson:2.10.1'
    testImplementation 'junit:junit:4.13.2'
    androidTestImplementation 'androidx.test.espresso:espresso-core:3.5.0'
}
```

#### Step 5: Run & Test
```bash
# Terminal in device-monitor-desktop/
npm install
npm start

# Android Studio: Click Run → Select Emulator/Device
```

## 📱 Android App Setup

### Create API Service

**File**: `device-monitor-android/app/src/main/java/com/devicemonitor/api/ApiService.java`

```java
import okhttp3.OkHttpClient;
import retrofit2.Retrofit;
import retrofit2.converter.gson.GsonConverterFactory;

public class ApiService {
    private static final String BASE_URL = "http://10.0.2.2:3000/";
    
    public static Retrofit getClient() {
        return new Retrofit.Builder()
            .baseUrl(BASE_URL)
            .addConverterFactory(GsonConverterFactory.create())
            .client(new OkHttpClient())
            .build();
    }
}
```

### Create Data Model

**File**: `device-monitor-android/app/src/main/java/com/devicemonitor/shared/ThreatModel.java`

```java
public class ThreatModel {
    public String name;
    public String risk;  // "High", "Medium", "Low"
    public String description;
    public String status;
    
    public ThreatModel(String name, String risk, String description) {
        this.name = name;
        this.risk = risk;
        this.description = description;
        this.status = "Active";
    }
}
```

### Create Main Activity

**File**: `device-monitor-android/app/src/main/java/com/devicemonitor/android/MainActivity.java`

```java
import android.os.Bundle;
import androidx.appcompat.app.AppCompatActivity;
import android.widget.Button;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {
    private Button scanButton;
    private TextView threatsList;
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        scanButton = findViewById(R.id.scanButton);
        threatsList = findViewById(R.id.threatsList);
        
        scanButton.setOnClickListener(v -> performScan());
    }
    
    private void performScan() {
        // Call desktop API to perform scan
        fetchThreatsFromDesktop();
    }
    
    private void fetchThreatsFromDesktop() {
        // Implement API call here
    }
}
```

## 🔧 Building the Apps

### Desktop App

```bash
cd device-monitor-desktop

# Development
npm install
npm start

# Build for Windows
npm run build:win

# Build for macOS
npm run build:mac

# Build for Linux
npm run build:linux
```

### Android App

```bash
# In Android Studio:
# Build → Build Bundle(s) / APK(s) → Build APK(s)

# Or from terminal:
./gradlew buildRelease
```

## 🌐 API Communication

### Desktop: Expose API

Add to `device-monitor-desktop/main.js`:

```javascript
const http = require('http');
const { scanSystem } = require('./system-scanner.js');

const apiServer = http.createServer(async (req, res) => {
  res.setHeader('Content-Type', 'application/json');
  res.setHeader('Access-Control-Allow-Origin', '*');
  
  if (req.url === '/api/scan' && req.method === 'POST') {
    const results = await scanSystem();
    res.writeHead(200);
    res.end(JSON.stringify(results));
  } else if (req.url === '/api/threats' && req.method === 'GET') {
    const data = await scanSystem();
    res.writeHead(200);
    res.end(JSON.stringify(data.threats));
  } else {
    res.writeHead(404);
    res.end(JSON.stringify({ error: 'Not found' }));
  }
});

apiServer.listen(3000, () => {
  console.log('API Server on http://localhost:3000');
});
```

### Mobile: Call API

```java
public void fetchThreatsFromDesktop() {
    OkHttpClient client = new OkHttpClient();
    Request request = new Request.Builder()
        .url("http://10.0.2.2:3000/api/threats")
        .build();
    
    client.newCall(request).enqueue(new Callback() {
        @Override
        public void onFailure(Call call, IOException e) {
            e.printStackTrace();
        }
        
        @Override
        public void onResponse(Call call, Response response) throws IOException {
            if (response.isSuccessful()) {
                String json = response.body().string();
                // Parse and update UI
                updateUI(json);
            }
        }
    });
}
```

## 📊 Testing Locally

### Desktop Testing

```bash
cd device-monitor-desktop
npm start
# App opens at http://localhost:3000 (desktop)
```

### Mobile Testing

1. **Emulator**: Android Studio → Run
2. **Physical Device**: 
   - Enable USB Debugging
   - Connect via USB
   - Android Studio → Run → Select Device
   - Change API URL to your computer IP: `http://192.168.X.X:3000`

## 🔒 Security Considerations

1. **CORS**: Add proper CORS headers in desktop API
2. **Authentication**: Implement token-based auth
3. **Encryption**: Use HTTPS for production
4. **Permissions**: Request necessary Android permissions in manifest

## 📤 Pushing to GitHub

```bash
git add .
git commit -m "Add Android module integration"
git push origin main

# Create release
git tag -a v1.0.0 -m "Initial release"
git push origin v1.0.0
```

## 🎯 Project Structure After Setup

```
transparency/
├── device-monitor-desktop/          # Electron desktop app
│   ├── main.js
│   ├── package.json
│   ├── system-scanner.js
│   ├── src/
│   │   ├── index.html
│   │   ├── renderer.js
│   │   └── styles.css
│   ├── README.md
│   └── LICENSE
├── device-monitor-android/          # Android app (new)
│   ├── app/
│   │   ├── src/main/
│   │   │   ├── java/com/devicemonitor/
│   │   │   │   ├── android/MainActivity.java
│   │   │   │   ├── api/ApiService.java
│   │   │   │   └── shared/ThreatModel.java
│   │   │   ├── res/
│   │   │   └── AndroidManifest.xml
│   │   └── build.gradle
│   ├── settings.gradle
│   └── local.properties
├── .github/workflows/
│   └── build.yml                    # CI/CD pipeline
├── CONTRIBUTING.md                  # Developer guide
├── RELEASES.md                      # Version history
├── ANDROID_STUDIO_INTEGRATION.md    # Setup guide
├── NEXT_STEPS.md                    # This file
└── README.md                        # Project overview
```

## 🐛 Troubleshooting

### "API not reachable from Android"
- Emulator: Use `10.0.2.2` instead of `localhost`
- Device: Use your PC's IP address (e.g., `192.168.1.100`)
- Firewall: Allow port 3000

### "Gradle sync fails"
- File → Sync Now
- Or: `./gradlew clean build`

### "npm install fails"
```bash
cd device-monitor-desktop
rm -rf node_modules package-lock.json
npm install --legacy-peer-deps
```

## 📚 Learning Resources

- [Electron Docs](https://www.electronjs.org/docs)
- [Android Development](https://developer.android.com/guide)
- [Retrofit HTTP Client](https://square.github.io/retrofit/)
- [REST API Best Practices](https://restfulapi.net/)

## ✨ Future Enhancements

- [ ] Firebase cloud sync
- [ ] Push notifications
- [ ] Biometric authentication
- [ ] Dark mode theme
- [ ] Advanced reporting
- [ ] Mobile app store deployment
- [ ] Web dashboard
- [ ] API documentation (Swagger)

## 🎓 Complete Command Reference

```bash
# Clone project
git clone https://github.com/MantheD27/transparency.git
cd transparency

# Desktop development
cd device-monitor-desktop
npm install --legacy-peer-deps
npm start

# Build installers
npm run build:win
npm run build:mac
npm run build:linux

# Git workflow
git add .
git commit -m "Your message"
git push origin main

# Create release tag
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0
```

