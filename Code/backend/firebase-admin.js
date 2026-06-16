const admin = require("firebase-admin");

let firebaseAdmin = null;

try {
  if (!process.env.FIREBASE_SERVICE_ACCOUNT_BASE64) {
    console.log("[FCM] Missing FIREBASE_SERVICE_ACCOUNT_BASE64. FCM disabled.");
  } else {
    const serviceAccount = JSON.parse(
      Buffer.from(
        process.env.FIREBASE_SERVICE_ACCOUNT_BASE64,
        "base64"
      ).toString("utf8")
    );

    firebaseAdmin = admin.initializeApp({
      credential: admin.credential.cert(serviceAccount),
    });

    console.log("[FCM] Firebase Admin initialized");
  }
} catch (err) {
  console.error("[FCM INIT ERROR]", err.message);
}

module.exports = firebaseAdmin;