import { useEffect } from "react";
import { useUser } from "@clerk/clerk-react";
import { supabase } from "../supabaseClient";

function SyncUser() {
  const { user, isLoaded } = useUser();

  useEffect(() => {
    if (!isLoaded || !user) return;

    const upsertUser = async () => {
      const { data, error } = await supabase
        .from("users")
        .upsert({
          clerk_user_id: user.id,
          email: user.primaryEmailAddress?.emailAddress || null,
        })
        .select(); // optioneel: return het aangemaakte/gewijzigde record

      if (error) {
        console.error("Error syncing user:", error);
      } else {
        console.log("User synced:", data);
      }
    };

    upsertUser();
  }, [isLoaded, user]);

  return null;
}

export default SyncUser;
