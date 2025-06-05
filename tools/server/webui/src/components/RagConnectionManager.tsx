import React, { useState, useMemo, useEffect } from 'react';
import {
  CONFIG_DEFAULT,
  RagConnection,
  getSelectedRagConnection,
} from '../Config';
import { isNumeric, isString } from '../utils/misc';
import {
  SettingsModalShortInput,
  SettingsModalShortRagInput,
} from './SettingDialog';
import StorageUtils from '../utils/storage';

// Key used for storing RAG schema status in localStorage
const RAG_SCHEMA_STORAGE_KEY = 'rag_db_schemas';

interface RagConnectionManagerProps {
  setLocalConfig: React.Dispatch<React.SetStateAction<typeof CONFIG_DEFAULT>>;
}

// Schema status tracking
interface RagSchemaStatus {
  [connectionId: string]: {
    exists: boolean;
    lastUpdated: number;
  };
}

export const RagConnectionManager: React.FC<RagConnectionManagerProps> = ({
  setLocalConfig,
}) => {
  // Load the actual config from localStorage to ensure we have the latest data
  const [localStorageConfig, setLocalStorageConfig] = useState<typeof CONFIG_DEFAULT>(
    StorageUtils.getConfig()
  );
  
  const [selectedDropdownOption, setSelectedDropdownOption] = useState(
    localStorageConfig.selected_rag_connection_name || '...'
  );

  const [currentConnectionName, setCurrentConnectionName] = useState(
    localStorageConfig.selected_rag_connection_name || ''
  );
  const [currentHost, setCurrentHost] = useState(
    getSelectedRagConnection(localStorageConfig).host
  );
  const [currentPort, setCurrentPort] = useState(
    getSelectedRagConnection(localStorageConfig).port
  );
  const [currentDbName, setCurrentDbName] = useState(
    getSelectedRagConnection(localStorageConfig).name
  );
  const [currentUser, setCurrentUser] = useState(
    getSelectedRagConnection(localStorageConfig).user
  );
  const [currentPassword, setCurrentPassword] = useState(
    getSelectedRagConnection(localStorageConfig).password || ''
  );

  // Helper function to reload config from localStorage
  const reloadConfigFromStorage = () => {
    const freshConfig = StorageUtils.getConfig();
    setLocalStorageConfig(freshConfig);
    return freshConfig;
  };

  // Helper function to get schema statuses from localStorage
  const getSchemaStatuses = (): RagSchemaStatus => {
    try {
      const data = localStorage.getItem(RAG_SCHEMA_STORAGE_KEY);
      return data ? JSON.parse(data) : {};
    } catch (error) {
      console.error('Error reading schema statuses from localStorage:', error);
      return {};
    }
  };

  // Helper function to save schema statuses to localStorage
  const saveSchemaStatus = (connectionId: string, exists: boolean) => {
    try {
      const currentStatuses = getSchemaStatuses();
      const updatedStatuses = {
        ...currentStatuses,
        [connectionId]: {
          exists,
          lastUpdated: Date.now(),
        },
      };
      localStorage.setItem(
        RAG_SCHEMA_STORAGE_KEY,
        JSON.stringify(updatedStatuses)
      );
    } catch (error) {
      console.error('Error saving schema status to localStorage:', error);
    }
  };

  // The dropdown options - now using localStorageConfig instead of CONFIG_DEFAULT
  const dropdownConnectionNames = useMemo(() => {
    const options: string[] = ['...'];
    if (
      localStorageConfig.rag_connections &&
      localStorageConfig.rag_connections.length > 0
    ) {
      const names = localStorageConfig.rag_connections.map(
        (conn) => conn.connection_name
      );
      return options.concat(names);
    }
    return options;
  }, [localStorageConfig.rag_connections]);

  // When the component mounts or localStorageConfig changes, update form values
  useEffect(() => {
    setSelectedDropdownOption(
      localStorageConfig.selected_rag_connection_name || '...'
    );
    
    // If a connection is selected, load its details
    if (localStorageConfig.selected_rag_connection_name) {
      const selectedConnection = localStorageConfig.rag_connections.find(
        (conn) => conn.connection_name === localStorageConfig.selected_rag_connection_name
      );
      
      if (selectedConnection) {
        setCurrentConnectionName(selectedConnection.connection_name);
        setCurrentHost(selectedConnection.host);
        setCurrentPort(selectedConnection.port);
        setCurrentDbName(selectedConnection.name);
        setCurrentUser(selectedConnection.user);
        setCurrentPassword(selectedConnection.password || '');
      }
    }
  }, [localStorageConfig]);

  // Handler for when the dropdown selection changes
  const handleDropdownChange = (e: React.ChangeEvent<HTMLSelectElement>) => {
    const newSelectedName = e.target.value;
    setSelectedDropdownOption(newSelectedName);

    // Update local input states based on dropdown selection
    if (newSelectedName === '...') {
      setCurrentConnectionName(''); // Clear for new connection
      setCurrentHost('<host>');
      setCurrentPort(5432);
      setCurrentDbName('<db>');
      setCurrentUser('<user>');
      setCurrentPassword('<password>');
    } else {
      // Get the latest config to ensure we have the most up-to-date connections
      const freshConfig = reloadConfigFromStorage();
      const selectedConnection = freshConfig.rag_connections.find(
        (conn) => conn.connection_name === newSelectedName
      );
      
      if (selectedConnection) {
        setCurrentConnectionName(selectedConnection.connection_name);
        setCurrentHost(selectedConnection.host);
        setCurrentPort(selectedConnection.port);
        setCurrentDbName(selectedConnection.name);
        setCurrentUser(selectedConnection.user);
        setCurrentPassword(selectedConnection.password || '');
        
        // Also update the main app config
        setLocalConfig((prevConfig) => ({
          ...prevConfig,
          selected_rag_connection_name: newSelectedName,
        }));
        
        // Update localStorage directly
        const updatedConfig = {
          ...freshConfig,
          selected_rag_connection_name: newSelectedName,
        };
        StorageUtils.setConfig(updatedConfig);
      }
    }
  };

  // Input field handlers
  const handleConnectionNameChange = (value: string) => {
    if (isString(value)) {
      setCurrentConnectionName(value.trim());
    }
  };

  const handleHostChange = (value: string) => {
    if (isString(value)) {
      setCurrentHost(value.trim());
    }
  };

  const handlePortChange = (value: string) => {
    if (isString(value) && isNumeric(value)) {
      setCurrentPort(Number(value));
    }
  };

  const handleDbNameChange = (value: string) => {
    if (isString(value)) {
      setCurrentDbName(value.trim());
    }
  };

  const handleUserChange = (value: string) => {
    if (isString(value)) {
      setCurrentUser(value.trim());
    }
  };

  const handlePasswordChange = (value: string) => {
    if (isString(value)) {
      setCurrentPassword(value.trim());
    }
  };

  const handleAddConnection = () => {
    // Use values from local states
    const newConnectionName = currentConnectionName.trim();
    const newHost = currentHost.trim();
    const newPort = currentPort;
    const newDbName = currentDbName.trim();
    const newUser = currentUser.trim();
    const newPassword = currentPassword || '';

    // Basic validation
    if (!newConnectionName || newConnectionName === '...') {
      alert('Please enter a valid Connection Name.');
      return;
    }
    if (!newHost) {
      alert('Please enter a valid Host.');
      return;
    }
    if (!isNumeric(newPort)) {
      alert('Please enter a valid Port number.');
      return;
    }
    if (!newDbName) {
      alert('Please enter a valid DB Name.');
      return;
    }
    if (!newUser) {
      alert('Please enter a valid User.');
      return;
    }

    // Get the latest config from localStorage to avoid conflicts
    const freshConfig = reloadConfigFromStorage();
    const currentConnections = freshConfig.rag_connections;
    const existingConnectionIndex = currentConnections.findIndex(
      (conn) => conn.connection_name === newConnectionName
    );

    let updatedConnections: RagConnection[];
    let feedbackMessage: string;
    let isNewConnection = false; // Flag to check if it's a new connection

    const newRagConnection: RagConnection = {
      id:
        existingConnectionIndex !== -1
          ? currentConnections[existingConnectionIndex].id
          : Date.now().toString(),
      connection_name: newConnectionName,
      host: newHost,
      port: newPort,
      name: newDbName,
      user: newUser,
      password: newPassword,
    };

    if (existingConnectionIndex !== -1) {
      updatedConnections = [
        ...currentConnections.slice(0, existingConnectionIndex),
        newRagConnection,
        ...currentConnections.slice(existingConnectionIndex + 1),
      ];
      feedbackMessage = `Connection "${newConnectionName}" updated successfully!`;
    } else {
      updatedConnections = [...currentConnections, newRagConnection];
      feedbackMessage = `Connection "${newConnectionName}" added successfully!`;
      isNewConnection = true; // Mark as new if added
    }

    // Update the config with the new/updated connection
    const updatedConfig = {
      ...freshConfig,
      rag_connections: updatedConnections,
      selected_rag_connection_name: newConnectionName, // Set the newly added/updated as selected
    };

    // Save to localStorage
    StorageUtils.setConfig(updatedConfig);
    
    // Update the local state with the new config
    setLocalStorageConfig(updatedConfig);
    
    // Update the main app state
    setLocalConfig(updatedConfig);

    // Display feedback message
    alert(feedbackMessage);

    // Optionally, create schema automatically for new connections
    if (isNewConnection) {
      handleCreateSchema(newRagConnection); // Pass the newly created connection details
    }

    // Also update the dropdown's internal state to reflect the new selection
    setSelectedDropdownOption(newConnectionName);
  };

  // REFACTORED: Create schema (storing status in localStorage)
  const handleCreateSchema = (connectionDetails?: RagConnection) => {
    // Use connectionDetails if provided, otherwise use current inputs
    const connectionToUse = connectionDetails || {
      connection_name: currentConnectionName,
      host: currentHost,
      port: currentPort,
      name: currentDbName,
      user: currentUser,
      password: currentPassword,
      id: Date.now().toString(), // Generate an ID if not provided
    };

    if (
      !connectionToUse.host ||
      !connectionToUse.name ||
      !connectionToUse.user
    ) {
      alert(
        'Cannot create schema: Missing connection details (host, name, user).'
      );
      return;
    }

    try {
      // Use the connection ID as the unique key
      const connectionId = connectionToUse.id || Date.now().toString();

      // Save schema status to localStorage
      saveSchemaStatus(connectionId, true);

      // Show success message
      alert(
        `Schema creation successful! Schema for "${connectionToUse.connection_name}" has been simulated and stored locally.`
      );
    } catch (error) {
      console.error('Error creating RAG DB schema:', error);
      alert('An error occurred while trying to create the RAG DB schema.');
    }
  };

  // REFACTORED: Check if schema exists (retrieving from localStorage)
  const handleCheckSchemaExists = () => {
    const connectionToUse = {
      connection_name: currentConnectionName,
      host: currentHost,
      port: currentPort,
      name: currentDbName,
      user: currentUser,
      password: currentPassword,
      id: '', // Will be populated from the found connection or generated
    };

    if (
      !connectionToUse.host ||
      !connectionToUse.name ||
      !connectionToUse.user
    ) {
      alert(
        'Cannot check schema: Missing connection details (host, name, user).'
      );
      return;
    }

    try {
      // Get the fresh config to ensure we have the latest connections
      const freshConfig = reloadConfigFromStorage();
      
      // Find the matching connection to get its ID
      const foundConnection = freshConfig.rag_connections.find(
        (conn) =>
          conn.connection_name === connectionToUse.connection_name &&
          conn.host === connectionToUse.host &&
          conn.port === connectionToUse.port &&
          conn.name === connectionToUse.name
      );

      // Use the found connection ID or generate a new one
      const connectionId = foundConnection?.id || Date.now().toString();

      // Get schema statuses from localStorage
      const schemaStatuses = getSchemaStatuses();
      const schemaInfo = schemaStatuses[connectionId];

      if (schemaInfo && schemaInfo.exists) {
        alert(
          `RAG Database schema exists. Last updated: ${new Date(schemaInfo.lastUpdated).toLocaleString()}`
        );
      } else {
        alert('RAG Database schema does NOT exist.');
      }
    } catch (error) {
      console.error('Error checking RAG DB schema existence:', error);
      alert('An error occurred while trying to check RAG DB schema existence.');
    }
  };

  // REFACTORED: Drop schema (removing from localStorage)
  const handleDropSchema = () => {
    const connectionToUse = {
      connection_name: currentConnectionName,
      host: currentHost,
      port: currentPort,
      name: currentDbName,
      user: currentUser,
      password: currentPassword,
    };

    if (
      !connectionToUse.host ||
      !connectionToUse.name ||
      !connectionToUse.user
    ) {
      alert(
        'Cannot drop schema: Missing connection details (host, name, user).'
      );
      return;
    }

    if (
      !confirm(
        'Are you sure you want to drop the RAG Database schema? This action is irreversible.'
      )
    ) {
      return; // User cancelled
    }

    try {
      // Get the fresh config to ensure we have the latest connections
      const freshConfig = reloadConfigFromStorage();
      
      // Find the matching connection to get its ID
      const foundConnection = freshConfig.rag_connections.find(
        (conn) =>
          conn.connection_name === connectionToUse.connection_name &&
          conn.host === connectionToUse.host &&
          conn.port === connectionToUse.port &&
          conn.name === connectionToUse.name
      );

      if (!foundConnection) {
        alert('Cannot find the connection to drop schema for.');
        return;
      }

      // Get schema statuses from localStorage
      const schemaStatuses = getSchemaStatuses();

      if (schemaStatuses[foundConnection.id]) {
        // Remove this schema status
        delete schemaStatuses[foundConnection.id];
        localStorage.setItem(
          RAG_SCHEMA_STORAGE_KEY,
          JSON.stringify(schemaStatuses)
        );
        alert('Schema drop successful!');
      } else {
        alert('No schema found to drop.');
      }
    } catch (error) {
      console.error('Error dropping RAG DB schema:', error);
      alert('An error occurred while trying to drop the RAG DB schema.');
    }
  };

  // REFACTORED: Dump config (now shows schema statuses too)
  const handleDumpConfig = () => {
    // Get the fresh config to ensure we have the latest data
    const freshConfig = reloadConfigFromStorage();
    const schemaStatuses = getSchemaStatuses();

    const debugInfo = {
      rag_connections: freshConfig.rag_connections,
      selected_rag_connection_name: freshConfig.selected_rag_connection_name,
      connection: getSelectedRagConnection(freshConfig),
      schema_statuses: schemaStatuses,
    };

    alert(JSON.stringify(debugInfo, null, 2));
  };

  return (
    <div className="mb-4 p-4 border rounded-lg shadow-sm bg-base-100">
      <h4 className="font-semibold text-md mb-3">RAG Connections</h4>

      <label className="form-control mb-2">
        <div className="label">
          <span className="label-text">Select Existing Connection</span>
        </div>
        <select
          className="select select-bordered w-full"
          value={selectedDropdownOption}
          onChange={handleDropdownChange}
        >
          {dropdownConnectionNames.map((name, index) => (
            <option key={index} value={name}>
              {name}
            </option>
          ))}
        </select>
      </label>

      {/* Input fields for connection details - now using local states */}
      <SettingsModalShortInput
        configKey="selected_rag_connection_name" // This is a bit misleading as it's the current 'Connection Name' field
        value={currentConnectionName}
        onChange={handleConnectionNameChange}
        label="Connection Name"
      />

      <SettingsModalShortRagInput
        ragKey="host"
        value={currentHost}
        onChange={handleHostChange}
        label="RAG DB Host"
      />
      <SettingsModalShortRagInput
        ragKey="port"
        value={currentPort}
        onChange={handlePortChange}
        label="RAG DB Port"
      />
      <SettingsModalShortRagInput
        ragKey="name"
        value={currentDbName}
        onChange={handleDbNameChange}
        label="RAG DB Name"
      />
      <SettingsModalShortRagInput
        ragKey="user"
        value={currentUser}
        onChange={handleUserChange}
        label="RAG DB User"
      />
      <SettingsModalShortRagInput
        ragKey="password"
        value={currentPassword}
        onChange={handlePasswordChange}
        label="RAG DB Password"
      />

      <button
        className="btn btn-primary w-full mt-4"
        onClick={handleAddConnection}
      >
        Save/Add Connection
      </button>

      {/* Buttons for RAG DB actions - now using localStorage instead of API */}
      <div className="flex gap-2 mt-2">
        <button
          className="btn btn-outline btn-sm flex-grow"
          onClick={() => handleCreateSchema()} // Call without specific connection to use current inputs
        >
          Create Schema
        </button>
        <button
          className="btn btn-outline btn-sm flex-grow"
          onClick={handleCheckSchemaExists}
        >
          Check Schema
        </button>
        <button
          className="btn btn-error btn-sm flex-grow" // btn-error for destructive action
          onClick={handleDropSchema}
        >
          Drop Schema
        </button>
        <button
          className="btn btn-outline btn-sm flex-grow"
          onClick={handleDumpConfig}
        >
          Dump Config
        </button>
      </div>
    </div>
  );
};
