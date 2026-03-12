import { auth, db } from "./firebase-config.js";
import {
  createUserWithEmailAndPassword,
  signInWithEmailAndPassword,
  signOut,
  onAuthStateChanged,
  updateProfile,
  sendPasswordResetEmail
} from "https://www.gstatic.com/firebasejs/11.4.0/firebase-auth.js";
import { doc, setDoc, getDoc, serverTimestamp } from "https://www.gstatic.com/firebasejs/11.4.0/firebase-firestore.js";

// DOM elements
const loginPage = document.getElementById("login-page");
const dashboardPage = document.getElementById("dashboard-page");
const loginForm = document.getElementById("login-form");
const signupForm = document.getElementById("signup-form");
const loginToggle = document.getElementById("login-toggle");
const signupToggle = document.getElementById("signup-toggle");
const logoutBtn = document.getElementById("logout-btn");
const authError = document.getElementById("auth-error");
const userDisplayName = document.getElementById("user-display-name");
const userEmail = document.getElementById("user-email");
const forgotPasswordLink = document.getElementById("forgot-password");

let currentUser = null;

// Toggle between login and signup forms
loginToggle.addEventListener("click", () => {
  loginForm.classList.remove("hidden");
  signupForm.classList.add("hidden");
  loginToggle.classList.add("active");
  signupToggle.classList.remove("active");
  clearError();
});

signupToggle.addEventListener("click", () => {
  signupForm.classList.remove("hidden");
  loginForm.classList.add("hidden");
  signupToggle.classList.add("active");
  loginToggle.classList.remove("active");
  clearError();
});

function showError(msg) {
  authError.textContent = msg;
  authError.classList.remove("hidden");
}

function clearError() {
  authError.textContent = "";
  authError.classList.add("hidden");
}

// Sign up
signupForm.addEventListener("submit", async (e) => {
  e.preventDefault();
  clearError();
  const name = document.getElementById("signup-name").value.trim();
  const email = document.getElementById("signup-email").value.trim();
  const password = document.getElementById("signup-password").value;

  if (password.length < 6) {
    showError("Password must be at least 6 characters.");
    return;
  }

  try {
    const cred = await createUserWithEmailAndPassword(auth, email, password);
    await updateProfile(cred.user, { displayName: name });
    // Create user document in Firestore
    await setDoc(doc(db, "users", cred.user.uid), {
      name: name,
      email: email,
      createdAt: serverTimestamp(),
      networks: [],
      settings: {
        theme: "dark",
        notifications: true
      }
    });
  } catch (err) {
    showError(friendlyError(err.code));
  }
});

// Login
loginForm.addEventListener("submit", async (e) => {
  e.preventDefault();
  clearError();
  const email = document.getElementById("login-email").value.trim();
  const password = document.getElementById("login-password").value;

  try {
    await signInWithEmailAndPassword(auth, email, password);
  } catch (err) {
    showError(friendlyError(err.code));
  }
});

// Forgot password
forgotPasswordLink.addEventListener("click", async (e) => {
  e.preventDefault();
  const email = document.getElementById("login-email").value.trim();
  if (!email) {
    showError("Enter your email address first, then click Forgot Password.");
    return;
  }
  try {
    await sendPasswordResetEmail(auth, email);
    showError("Password reset email sent. Check your inbox.");
    authError.style.color = "#00e5ff";
  } catch (err) {
    showError(friendlyError(err.code));
  }
});

// Logout
logoutBtn.addEventListener("click", async () => {
  await signOut(auth);
});

// Auth state observer
onAuthStateChanged(auth, (user) => {
  if (user) {
    currentUser = user;
    loginPage.classList.add("hidden");
    dashboardPage.classList.remove("hidden");
    userDisplayName.textContent = user.displayName || user.email;
    if (userEmail) userEmail.textContent = user.email || "";
    // Load dashboard data
    if (window.loadDashboard) window.loadDashboard(user);
  } else {
    currentUser = null;
    loginPage.classList.remove("hidden");
    dashboardPage.classList.add("hidden");
  }
});

function friendlyError(code) {
  const map = {
    "auth/email-already-in-use": "An account with this email already exists.",
    "auth/invalid-email": "Invalid email address.",
    "auth/weak-password": "Password is too weak (min 6 characters).",
    "auth/user-not-found": "No account found with this email.",
    "auth/wrong-password": "Incorrect password.",
    "auth/invalid-credential": "Invalid email or password.",
    "auth/too-many-requests": "Too many attempts. Try again later."
  };
  return map[code] || "Authentication error. Please try again.";
}

export { currentUser };
