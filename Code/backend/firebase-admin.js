const { initializeApp, getApps, cert } = require("firebase-admin/app");
const { getMessaging } = require("firebase-admin/messaging");

let firebaseAdmin = null;

try {
  const encoded = process.env.FIREBASE_SERVICE_ACCOUNT_BASE64;

  if (!encoded) {
    console.log("[FCM] Missing FIREBASE_SERVICE_ACCOUNT_BASE64. FCM disabled.");
  } else {
    const serviceAccount = JSON.parse(
      Buffer.from(encoded, "base64").toString("utf8")
    );

    if (getApps().length === 0) {
      initializeApp({
        credential: cert(serviceAccount),
      });
    }

    firebaseAdmin = {
      messaging: () => getMessaging(),
    };

    console.log("[FCM] Firebase Admin initialized");
  }
} catch (err) {
  console.error("[FCM INIT ERROR]", err.message);
}

module.exports = firebaseAdmin;