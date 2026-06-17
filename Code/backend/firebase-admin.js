const admin = require("firebase-admin");

let firebaseAdmin = null;

try {
  const encoded = process.env.FIREBASE_SERVICE_ACCOUNT_BASE64;

  if (!encoded) {
    console.log("[FCM] Missing FIREBASE_SERVICE_ACCOUNT_BASE64. FCM disabled.");
  } else {
    const serviceAccount = JSON.parse(
      Buffer.from(encoded, "base64").toString("utf8")
    );

    if (!admin.apps.length) {
      admin.initializeApp({
        credential: admin.credential.cert(serviceAccount),
      });
    }

    firebaseAdmin = admin;

    console.log("[FCM] Firebase Admin initialized");
  }
} catch (err) {
  console.error("[FCM INIT ERROR]", err.message);
}

module.exports = firebaseAdmin;