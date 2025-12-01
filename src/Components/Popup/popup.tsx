import React from "react";
import "./popup.css";
import popupImage from "../../Assets/Images/image.png";

type ButtonVariant = "primary" | "secondary" | "danger" | "outline";

export interface PopupButton {
  label: string;
  onClick?: () => void;
  variant?: ButtonVariant;
  /** if false: clicking this button will NOT close the popup; default is to close */
  closes?: boolean;
}

interface PopupProps {
  title?: string;
  message?: string;
  children?: React.ReactNode;
  buttons?: PopupButton[]; // up to 4
  onClose: () => void;
}

const Popup: React.FC<PopupProps> = ({
  title,
  message,
  children,
  buttons,
  onClose,
}) => {
  const MAX_BUTTONS = 4;
  const userButtons = (buttons ?? []).slice(0, MAX_BUTTONS - 1);

  const handleUserButtonClick = (btn: PopupButton) => {
    try {
      if (btn.onClick) btn.onClick();
    } finally {
      if (btn.closes !== false) onClose();
    }
  };

  const handleClose = () => onClose();

  return (
    <div
      className="popup-overlay"
      onClick={onClose}
      role="dialog"
      aria-modal="true"
      aria-labelledby={title ? "popup-title" : undefined}
    >
      <div className="popup-content" onClick={(e) => e.stopPropagation()}>
        {/* Left image column */}
        <div className="popup-left">
          <img src={popupImage} alt="Popup visual" className="popup-image" />
        </div>

        {/* Right content */}
        <div className="popup-right">
          {title && (
            <h2 className="popup-title" id="popup-title">
              {title}
            </h2>
          )}

          {message && <p className="popup-message">{message}</p>}

          {children}

          <div className="popup-footer">
            <div className="popup-footer-left">
              <button
                type="button"
                className={`popup-button popup-button--outline popup-close-left`}
                onClick={handleClose}
              >
                Close
              </button>
            </div>

            <div className="popup-footer-right">
              {userButtons.map((btn, idx) => (
                <button
                  key={idx}
                  type="button"
                  className={`popup-button popup-button--${
                    btn.variant ?? "primary"
                  }`}
                  onClick={() => handleUserButtonClick(btn)}
                >
                  {btn.label}
                </button>
              ))}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default Popup;
