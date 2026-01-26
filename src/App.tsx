import React from "react";
import logo from "./logo.svg";
import "./App.css";
import { BrowserRouter, Route, Routes } from "react-router-dom";
import { ClerkProvider } from "@clerk/clerk-react";

import NavBar from "./Components/NavBar/navbar";
import Footer from "./Components/Footer/footer";

import Home from "./Pages/Home/home";
import SignIn from "./Pages/SignIn/SignIn";
import Stats from "./Pages/Stats/Stats";
import About from "./Pages/About/about";
import ScoreBoard from "./Pages/ScoreBoard/scoreboard";
import SyncUser from "./Hooks/SyncUser";
import { EspDataProvider } from "./Contexts/EspDataContext";

function App() {
  const clerkKey = process.env.REACT_APP_CLERK_PUBLISHABLE_KEY as
    | string
    | undefined;
  if (!clerkKey) {
    throw new Error(
      "Missing REACT_APP_CLERK_PUBLISHABLE_KEY. Add it to a .env file at the project root and restart `npm start`.",
    );
  }
  console.log("CLERK KEY:", clerkKey ? "[set]" : "[missing]");
  return (
    <React.StrictMode>
      <ClerkProvider publishableKey={clerkKey}>
        <SyncUser />
        <EspDataProvider>
          <BrowserRouter basename={process.env.PUBLIC_URL}>
            <NavBar />
            <Routes>
              <Route path="/" element={<Home />} />
              <Route path="/sign-in/" element={<SignIn />} />
              <Route path="/About" element={<About />} />
              <Route path="/Stats" element={<Stats />} />
              <Route path="/ScoreBoard" element={<ScoreBoard />} />
            </Routes>
            <Footer />
          </BrowserRouter>
        </EspDataProvider>
      </ClerkProvider>
    </React.StrictMode>
  );
}

export default App;
