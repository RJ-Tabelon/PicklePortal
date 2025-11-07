// Import Firebase SDK modules
import { initializeApp } from "firebase/app";
import { getDatabase } from "firebase/database";

const firebaseConfig = {
  // REPLACE WITH YOUR ACTUAL CONFIG VALUES
  apiKey: "your-api-key", 
  authDomain: "pickleportal.firebaseapp.com",
  databaseURL: "your-database-url",
  projectId: "pickleportal", 
  storageBucket: "pickleportal.firebasestorage.app",
  messagingSenderId: "716167683231",
  appId: "1:716167683231:web:bdc5044d5699e2fa4c96aa",
  measurementId: "G-4924RGGSXG"
};

const app = initializeApp(firebaseConfig);

export const db = getDatabase(app);

console.log("Firebase Realtime Database initialized.");
