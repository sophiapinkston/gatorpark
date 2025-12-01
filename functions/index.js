/**
 * Import function triggers from their respective submodules:
 *
 * const {onCall} = require("firebase-functions/v2/https");
 * const {onDocumentWritten} = require("firebase-functions/v2/firestore");
 *
 * See a full list of supported triggers at https://firebase.google.com/docs/functions
 */

const express = require("express");
const {setGlobalOptions} = require("firebase-functions");
const {onRequest} = require("firebase-functions/https");
const logger = require("firebase-functions/logger");

// For cost control, you can set the maximum number of containers that can be
// running at the same time. This helps mitigate the impact of unexpected
// traffic spikes by instead downgrading performance. This limit is a
// per-function limit. You can override the limit for each function using the
// `maxInstances` option in the function's options, e.g.
// `onRequest({ maxInstances: 5 }, (req, res) => { ... })`.
// NOTE: setGlobalOptions does not apply to functions using the v1 API. V1
// functions should each use functions.runWith({ maxInstances: 10 }) instead.
// In the v1 API, each function can only serve one request per container, so
// this will be the maximum concurrent request count.
setGlobalOptions({ maxInstances: 10 });
// Key is username
// Value is tuple {password, email}
const db = new Map();

const app = express();

// Middleware, allows for post data parsing
app.use(express.urlencoded({ extended: true }));
app.use(express.json());

app.post('/api/register', (req, res) => {
  const {email, username, password} = req.body;
  const regex = /^[a-zA-Z0-9._%+-]+@ufl\.edu$/;
  if (!regex.test(email)) {
    return res.send("Invalid UFL email");
  } 
  if (db.has(username)) {
    return res.send("Username already taken");
  } 
  db.set(username, [password, email]);
  return res.redirect('/login');
});

app.post('/api/login', (req, res) => {
  const {username, password} = req.body;
  if (!db.has(username)) {
    return res.send("Username does not exist.");
  } 
  const user = db.get(username);
  if (user[0] === password) {
        return res.redirect('/dashboard');
    } else {
        return res.send("Password incorrect");
    }
});

exports.api = onRequest(app);

 
// Create and deploy your first functions
// https://firebase.google.com/docs/functions/get-started

// exports.helloWorld = onRequest((request, response) => {
//   logger.info("Hello logs!", {structuredData: true});
//   response.send("Hello from Firebase!");
// });
