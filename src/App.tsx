import React from "react";
import logo from "./logo.svg";
import "./App.css";
import { BrowserRouter, Route, Routes } from "react-router-dom";

import NavBar from "./Components/NavBar/navbar";
import Footer from "./Components/Footer/footer";

import Home from "./Pages/Home/home";
import About from "./Pages/About/about";
import ScoreBoard from "./Pages/ScoreBoard/scoreboard";
import { EspDataProvider } from "./Contexts/EspDataContext";

function App() {
  return (
    <EspDataProvider>
      <BrowserRouter basename={process.env.PUBLIC_URL}>
        <NavBar />
        <Routes>
          <Route path="/" element={<Home />} />
          <Route path="/About" element={<About />} />
          <Route path="/ScoreBoard" element={<ScoreBoard />} />
        </Routes>
        <Footer />
      </BrowserRouter>
    </EspDataProvider>
  );
}

export default App;
