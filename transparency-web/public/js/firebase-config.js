import { initializeApp } from "https://www.gstatic.com/firebasejs/11.4.0/firebase-app.js";
import { getAnalytics } from "https://www.gstatic.com/firebasejs/11.4.0/firebase-analytics.js";
import { getAuth } from "https://www.gstatic.com/firebasejs/11.4.0/firebase-auth.js";
import { getFirestore } from "https://www.gstatic.com/firebasejs/11.4.0/firebase-firestore.js";

const firebaseConfig = {
  apiKey: "AIzaSyAIvpRcWnSGyAZdw3kS9P4yTD97SN2w690",
  authDomain: "transparency-2920f.firebaseapp.com",
  projectId: "transparency-2920f",
  storageBucket: "transparency-2920f.firebasestorage.app",
  messagingSenderId: "900987949143",
  appId: "1:900987949143:web:10ac43298f9bf8f15cd312",
  measurementId: "G-Q80TNJY4GW"
};

const app = initializeApp(firebaseConfig);
const analytics = getAnalytics(app);
const auth = getAuth(app);
const db = getFirestore(app);

export { app, analytics, auth, db };
