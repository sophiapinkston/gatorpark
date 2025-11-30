
// Imports
 import express from 'express';
 import path from 'path';
 import { fileURLToPath } from 'url';
 const __filename = fileURLToPath(import.meta.url);
 const __dirname = path.dirname(__filename);

const db = new Map();

const app = express();

// Middleware, allows for post data parsing
app.use(express.urlencoded({ extended: true }));
app.use(express.json());


// Landing page
app.use(express.static('public/landing'));
app.get('/landing', (req, res) => {
  res.sendFile(path.join(__dirname, "/public/landing/landing.html"));
});

// Login page
app.use(express.static('public/login'));
app.get('/login', (req, res) => {
  res.sendFile(path.join(__dirname, "/public/login/login.html"));
});
app.post('/login', (req, res) => {
  const {username, password} = req.body;
  if (!db.has(username)) {
    return res.send("Username does not exist.");
  } 
  const user = db.get(username);
  if (user.password === password) {
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

app.post('/register', (req, res) => {
  const {username, password} = req.body;
  if (db.has(username)) {
    return res.send("Username already taken");
  } 
  db.set(username, { password });
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
    

   
