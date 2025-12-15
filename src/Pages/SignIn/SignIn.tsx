import "./SignIn.css";
import React from "react";
import { SignIn as ClerkSignIn } from "@clerk/clerk-react";

function SignIn() {
  return (
    <div className="signin-page">
      {/* Background blobs */}
      <div className="blob blob-1"></div>
      <div className="blob blob-2"></div>
      <div className="blob blob-3"></div>

      <div className="signin-container">
        <div className="signin-text">
          <h1>Welcome back</h1>
          <p>Log in to continue your SmartKorf journey</p>
        </div>

        <div className="signin-card">
          <ClerkSignIn
            appearance={{
              elements: {
                card: "clerk-card",
                headerTitle: "hidden",
                headerSubtitle: "hidden",
              },
            }}
            redirectUrl="/SmartKorf"
            afterSignInUrl="/SmartKorf"
          />
        </div>
      </div>
    </div>
  );
}

export default SignIn;
