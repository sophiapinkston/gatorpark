
 
 import express from 'express';
 import path from 'path';
 import { fileURLToPath } from 'url';
 const __filename = fileURLToPath(import.meta.url);
 const __dirname = path.dirname(__filename);
 import nodemailer from 'nodemailer';


// Key is username
// Value is tuple {password, email}
const db = new Map();

const app = express();

// Middleware, allows for post data parsing
app.use(express.urlencoded({ extended: true }));
app.use(express.json());


// Landing page
app.use(express.static('public/'));
app.get('/landing', (req, res) => {
  res.sendFile(path.join(__dirname, "/public/index.html"));
});

// Login page
app.use(express.static('public/login'));
app.get('/login', (req, res) => {
  res.sendFile(path.join(__dirname, "/public/login/login.html"));
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
// MFA page
app.use(express.static('public/mfa'));
app.get('/mfa', (req, res) => {
  res.sendFile(path.join(__dirname, "/public/mfa/mfa.html"));
});
app.post('/mfa', (req, res) => {
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

// Register page
app.use(express.static('public/register'));
app.get('/register', (req, res) => {
  res.sendFile(path.join(__dirname, "/public/register/register.html"));
});
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

// Dashboard page
app.use(express.static('public/dashboard'));
app.get('/dashboard', (req, res) => {
  res.sendFile(path.join(__dirname, "/public/dashboard/index.html"));
});


// Server
var server = app.listen(5000, function() {
    console.log('listening to requests on port 5000');
});
    
   

