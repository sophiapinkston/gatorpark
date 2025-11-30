
// Imports
 import express from 'express';
 import path from 'path';
 import { fileURLToPath } from 'url';
 const __filename = fileURLToPath(import.meta.url);
 const __dirname = path.dirname(__filename);
import { JsonDB } from "node-json-db";
import { Config } from "node-json-db/dist/lib/JsonDBConfig.js";

// Create database
const db = new JsonDB(new Config("myDataBase", true, false, "/"));

const app = express();

// Middleware, allows for post data parsing
app.use(express.urlencoded({ extended: true }));


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
  res.send(username);
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
    

   
