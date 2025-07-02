import React, { useState, useEffect, useCallback } from 'react';
import { /*AppContextProvider,*/ useAppContext } from '../utils/app.context';

export function ServerInfo2() {
  // Get server properties and the setter from your application context
  const { serverProps /*, setServerProps*/ } = useAppContext();

  // State to control the visibility of the model selection dropdown
  const [showModelDropdown, setShowModelDropdown] = useState(false);
  // State to store the list of available models fetched from the backend
  const [availableModels, setAvailableModels] = useState<string[]>([]);
  // State to indicate if the model list is currently being loaded from the backend
  const [isLoadingModels, setIsLoadingModels] = useState(false);
  // State to store any error messages from backend calls (e.g., failed to fetch list, failed to change model)
  const [modelError, setModelError] = useState<string | null>(null);
  // State to indicate if a model change request is currently in progress
  const [isChangingModel, setIsChangingModel] = useState(false);

  // Helper to extract just the filename from the full model_path
  const currentModelFilename =
    serverProps?.model_path?.split(/(\\|\/)/).pop() || 'N/A';

  /**
   * Fetches the list of available models from the backend.
   * This function is wrapped in `useCallback` to prevent unnecessary re-creations,
   * which can be beneficial for performance in larger components.
   */
  const fetchAvailableModels = useCallback(async () => {
    setIsLoadingModels(true); // Indicate loading has started
    setModelError(null); // Clear any previous errors
    try {
      const response = await fetch('/model-action', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ action: 'list-models' }), // JSON body for listing models
      });

      if (!response.ok) {
        // If the HTTP response is not successful (e.g., 404, 500), throw an error
        const errorText = await response.text();
        throw new Error(
          `HTTP error! Status: ${response.status} - ${errorText}`
        );
      }

      const data = await response.json();
      // Validate that the response contains an array of models
      if (data && Array.isArray(data.models)) {
        setAvailableModels(data.models); // Update state with the fetched models
      } else {
        throw new Error(
          'Invalid response format: "models" array not found or not an array.'
        );
      }
    } catch (error: any) {
      console.error('Failed to fetch available models:', error);
      setModelError(`Failed to load models: ${error.message}`); // Set the error message
    } finally {
      setIsLoadingModels(false); // Reset loading state regardless of success or failure
    }
  }, []); // Empty dependency array means this function is created once and reused

  /**
   * Handles the click event on the "Model" display text.
   * Toggles the dropdown visibility and fetches models if they haven't been loaded yet.
   */
  const handleModelClick = () => {
    // Prevent interaction if another action (loading/changing) is already in progress
    if (isLoadingModels || isChangingModel) return;

    // Toggle the dropdown's visibility
    setShowModelDropdown((prev) => !prev);

    // If the dropdown is about to be shown AND we don't have models loaded yet,
    // then trigger the fetch operation.
    if (!showModelDropdown && availableModels.length === 0) {
      fetchAvailableModels();
    }
  };

  /**
   * Handles the change event when a new option is selected in the dropdown.
   * Sends a request to the backend to change the active model.
   */
  const handleModelChange = async (
    event: React.ChangeEvent<HTMLSelectElement>
  ) => {
    const selectedModel = event.target.value;

    // Close the dropdown immediately after a selection attempt
    setShowModelDropdown(false);

    // If the selected model is the same as the currently active one, do nothing
    if (selectedModel === currentModelFilename) {
      console.log('Selected the current model, no action needed.');
      return;
    }

    setIsChangingModel(true); // Indicate that a model change is in progress
    setModelError(null); // Clear any previous errors
    try {
      const response = await fetch('/model-action', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ action: 'change-model', model: selectedModel }), // JSON body for changing model
      });

      if (!response.ok) {
        // Attempt to parse backend error message, fallback to generic HTTP error
        const errorData = await response
          .json()
          .catch(() => ({ message: `HTTP error! Status: ${response.status}` }));
        throw new Error(
          errorData.message ||
            `Failed to change model. Status: ${response.status}`
        );
      }

      const data = await response.json();
      // Assuming a successful response includes a 'success' flag and 'new_model_path'
      if (data.success && data.new_model_path && serverProps) {
        serverProps.model_path = data.new_model_path;
        console.log(`Successfully changed model to: ${data.new_model_path}`);
      } else {
        // Handle cases where backend indicates failure without throwing HTTP error
        throw new Error(
          data.message ||
            'Failed to change model without specific success indication.'
        );
      }
    } catch (error: any) {
      console.error('Failed to change model:', error);
      setModelError(`Failed to change model: ${error.message}`); // Set error message
    } finally {
      setIsChangingModel(false); // Reset changing model state
    }
  };

  /**
   * useEffect hook to handle clicks outside the model display and dropdown,
   * which should close the dropdown. This improves user experience.
   */
  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      const modelElement = document.getElementById('model-display');
      const dropdownElement = document.getElementById('model-dropdown');

      // If the dropdown is currently open AND the click happened outside both the
      // clickable model text AND the dropdown itself, then close the dropdown.
      if (
        showModelDropdown &&
        modelElement &&
        dropdownElement &&
        !modelElement.contains(event.target as Node) &&
        !dropdownElement.contains(event.target as Node)
      ) {
        setShowModelDropdown(false);
      }
    };

    // Add the event listener when the dropdown is shown
    document.addEventListener('mousedown', handleClickOutside);

    // Clean up the event listener when the component unmounts or
    // when `showModelDropdown` changes (to re-attach if needed).
    return () => {
      document.removeEventListener('mousedown', handleClickOutside);
    };
  }, [showModelDropdown]); // Dependency array: re-run effect if showModelDropdown changes

  return (
    <div className="card card-sm shadow-sm border-1 border-base-content/20 text-base-content/70 mb-6 rounded-lg">
      <div className="card-body p-4">
        <b className="text-lg text-base-content">Server Info</b>
        <p className="mt-2 text-sm">
          <div className="flex items-center space-x-2">
            <b className="text-base-content">Model</b>:
            <span
              id="model-display" // Unique ID for targeting this element in handleClickOutside
              className={`cursor-pointer text-blue-500 hover:underline relative ${isLoadingModels || isChangingModel ? 'opacity-70 cursor-not-allowed' : ''}`}
              onClick={handleModelClick} // The click handler for making it interactive
              title="Click to change model" // Tooltip for user
            >
              {currentModelFilename}
              {isLoadingModels && (
                <span className="ml-2 text-xs text-gray-500">
                  (loading models...)
                </span>
              )}
              {isChangingModel && (
                <span className="ml-2 text-xs text-gray-500">
                  (changing model...)
                </span>
              )}
            </span>
            {/* Conditionally render the dropdown when showModelDropdown is true */}
            {showModelDropdown && (
              <div
                id="model-dropdown" // Unique ID for targeting this element in handleClickOutside
                // Positioning the dropdown absolutely relative to its parent 'p' tag or 'div'
                className="absolute z-10 mt-2 bg-white border border-gray-300 rounded-md shadow-lg"
                // You might need to adjust 'top', 'left' based on your specific layout for perfect alignment
                style={{ top: 'auto', left: 'auto' }}
              >
                <select
                  className="block w-full p-2 border-none rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
                  onChange={handleModelChange} // Handle selection change
                  value={currentModelFilename} // Set the current model as the initially selected option
                  disabled={isLoadingModels || isChangingModel} // Disable interaction while busy
                >
                  {/* Conditional options for feedback */}
                  {isLoadingModels && (
                    <option value="">Loading models...</option>
                  )}
                  {modelError && (
                    <option value="" className="text-red-500">
                      Error loading models
                    </option>
                  )}

                  {/* If no models found after loading, display a message */}
                  {!isLoadingModels &&
                    !modelError &&
                    availableModels.length === 0 && (
                      <option value="">No models found</option>
                    )}

                  {/* Map over the available models to create dropdown options */}
                  {availableModels.map((model) => (
                    <option key={model} value={model}>
                      {model}
                    </option>
                  ))}
                </select>
              </div>
            )}
          </div>
          <br />
          <b>Build</b>: {serverProps?.build_info || 'N/A'}
          <br />
        </p>
        {/* Display any general error messages */}
        {modelError && (
          <p className="text-red-500 text-sm mt-2">Error: {modelError}</p>
        )}
      </div>
    </div>
  );
}
