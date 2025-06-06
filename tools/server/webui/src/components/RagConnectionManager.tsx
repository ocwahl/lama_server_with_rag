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
import { Button } from '@/components/ui/button';
import { Label } from '@/components/ui/label';

interface RagConnectionManagerProps {
  setLocalConfig: React.Dispatch<React.SetStateAction<typeof CONFIG_DEFAULT>>;
}

export const RagConnectionManager: React.FC<RagConnectionManagerProps> = ({
  setLocalConfig,
}) => {
    const [selectedDropdownOption, setSelectedDropdownOption] = useState(
      CONFIG_DEFAULT.selected_rag_connection_name || '...'
    );
  
    const [currentConnectionName, setCurrentConnectionName] = useState(
      CONFIG_DEFAULT.selected_rag_connection_name
    );
    const [currentHost, setCurrentHost] = useState(
      getSelectedRagConnection(CONFIG_DEFAULT).host
    );
    const [currentPort, setCurrentPort] = useState(
      getSelectedRagConnection(CONFIG_DEFAULT).port
    );
    const [currentDbName, setCurrentDbName] = useState(
      getSelectedRagConnection(CONFIG_DEFAULT).name
    );
    const [currentUser, setCurrentUser] = useState(
      getSelectedRagConnection(CONFIG_DEFAULT).user
    );
    const [currentPassword, setCurrentPassword] = useState(
      getSelectedRagConnection(CONFIG_DEFAULT).password || ''
    );
  
    // 2. Use useMemo to derive the actual list of options for the dropdown.
    // This array will contain strings like ["Main DB", "Dev Instance"].
    const dropdownConnectionNames = useMemo(() => {
      const options: string[] = ['...'];
      if (
        CONFIG_DEFAULT.rag_connections &&
        CONFIG_DEFAULT.rag_connections.length > 0
      ) {
        const names = CONFIG_DEFAULT.rag_connections.map(
          (conn) => conn.connection_name
        );
        return options.concat(names);
      }
      return options;
    }, [CONFIG_DEFAULT.rag_connections]);
  
    // Optional: If localConfig.selected_rag_connection_name can change externally
    // (e.g., from a different part of the app updating localConfig),
    // you might want to sync selectedDropdownOption with it.
    useEffect(() => {
      // This useEffect is for when localConfig.selected_rag_connection_name changes externally
      // or when the component mounts, ensuring local input states are synced.
      setSelectedDropdownOption(
        CONFIG_DEFAULT.selected_rag_connection_name || '...'
      );
    }, [
      CONFIG_DEFAULT.selected_rag_connection_name,
      CONFIG_DEFAULT.rag_connections,
    ]); // Added rag_connections as dependency
  
    // Handler for when the dropdown selection changes
    const handleDropdownChange = (e: React.ChangeEvent<HTMLSelectElement>) => {
      const newSelectedName = e.target.value;
      setSelectedDropdownOption(newSelectedName);
  
      // Update local input states based on dropdown selection
      if (newSelectedName === '...') {
        setCurrentConnectionName(''); // Clear for new connection
        setCurrentHost('<host>');
        setCurrentPort(5432);
        setCurrentDbName('<name>');
        setCurrentUser('<user>');
        setCurrentPassword('<password>');
      } else {
        const selectedConnection = CONFIG_DEFAULT.rag_connections.find(
          (conn) => conn.connection_name === newSelectedName
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
  
      // IMPORTANT: DO NOT update localConfig here.
      // localConfig.selected_rag_connection_name will be updated
      // only when Save/Add Connection is clicked.
      // However, if the dropdown change should immediately persist the selection
      // in localConfig (independent of the field values), you could uncomment this:
      /*
      setLocalConfig((prevConfig) => ({
        ...prevConfig,
        selected_rag_connection_name: newSelectedName,
      }));
      */
    };
  
    // --- MODIFIED: handleRagDbFieldChange to update local states ---
    // Note: We don't need a `configKey` mapping here anymore,
    // we directly update the specific local state.
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
        // Check if string and numeric
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
      console.log(
        JSON.stringify({
          newConnectionName: newConnectionName,
          newHost: newHost,
          newPort: newPort,
          newDbName: newDbName,
          newUser: newUser,
          newPassword: newPassword,
        })
      );
  
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
  
      setLocalConfig((prevConfig) => {
        console.log(
          JSON.stringify({
            prevConfig: prevConfig,
          })
        );
        const currentConnections = prevConfig.rag_connections;
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
  
        // Update localConfig with the new rag_connections array and the selected connection name
        const updatedConfig = {
          ...prevConfig,
          rag_connections: updatedConnections,
          selected_rag_connection_name: newConnectionName, // Set the newly added/updated as selected
        };
  
        // Display feedback message
        console.log(
          JSON.stringify({
            newConfig: updatedConfig,
          })
        );
        alert(feedbackMessage);
  
        // Optionally, trigger schema creation if it's a new connection
        if (isNewConnection) {
          handleCreateSchema(newRagConnection); // Pass the newly created connection details
        }
  
        return updatedConfig;
      });
  
      // Also update the dropdown's internal state to reflect the new selection
      setSelectedDropdownOption(newConnectionName);
    };
  
    // --- NEW: Function to create RAG database schema ---
    const handleCreateSchema = async (connectionDetails?: RagConnection) => {
      // Use connectionDetails if provided (e.g., from handleAddConnection),
      // otherwise use the current local states for the connection.
      const connectionToUse = connectionDetails || {
        connection_name: currentConnectionName,
        host: currentHost,
        port: currentPort,
        name: currentDbName,
        user: currentUser,
        password: currentPassword,
        id: '', // ID is not strictly needed for the API call
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
        const response = await fetch('/rag_db_admin', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({
            action: 'create',
            rag_connection: {
              host: connectionToUse.host,
              port: connectionToUse.port,
              name: connectionToUse.name,
              user: connectionToUse.user,
              password: connectionToUse.password,
            },
          }),
        });
  
        if (response.ok) {
          const result = await response.json();
          alert(`Schema creation successful: ${result.message}`);
        } else {
          const errorData = await response.json();
          alert(
            `Failed to create schema: ${errorData.message || response.statusText}`
          );
        }
      } catch (error) {
        console.error('Error creating RAG DB schema:', error);
        alert('An error occurred while trying to create the RAG DB schema.');
      }
    };
  
    // --- NEW: Function to check if schema exists ---
    const handleCheckSchemaExists = async () => {
      const connectionToUse = {
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
          'Cannot check schema: Missing connection details (host, name, user).'
        );
        return;
      }
  
      try {
        const response = await fetch('/rag_db_admin', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({
            action: 'exists',
            rag_connection: {
              host: connectionToUse.host,
              port: connectionToUse.port,
              name: connectionToUse.name,
              user: connectionToUse.user,
              password: connectionToUse.password,
            },
          }),
        });
  
        if (response.ok) {
          const result = await response.json();
          if (result.exists) {
            alert('RAG Database schema exists.');
          } else {
            alert('RAG Database schema does NOT exist.');
          }
        } else {
          const errorData = await response.json();
          alert(
            `Failed to check schema existence: ${errorData.message || response.statusText}`
          );
        }
      } catch (error) {
        console.error('Error checking RAG DB schema existence:', error);
        alert('An error occurred while trying to check RAG DB schema existence.');
      }
    };
  
    // --- NEW: Function to drop schema ---
    const handleDropSchema = async () => {
      const connectionToUse = {
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
        const response = await fetch('/rag_db_admin', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({
            action: 'drop',
            rag_connection: {
              host: connectionToUse.host,
              port: connectionToUse.port,
              name: connectionToUse.name,
              user: connectionToUse.user,
              password: connectionToUse.password,
            },
          }),
        });
  
        if (response.ok) {
          const result = await response.json();
          alert(`Schema drop successful: ${result.message}`);
        } else {
          const errorData = await response.json();
          alert(
            `Failed to drop schema: ${errorData.message || response.statusText}`
          );
        }
      } catch (error) {
        console.error('Error dropping RAG DB schema:', error);
        alert('An error occurred while trying to drop the RAG DB schema.');
      }
    };
  
    // --- NEW: Function to drop schema ---
    const handleDumpConfig = async () => {
      const body = JSON.stringify({
        rag_connections: CONFIG_DEFAULT.rag_connections,
        selected_rag_connection_name: CONFIG_DEFAULT.selected_rag_connection_name,
        connection: getSelectedRagConnection(CONFIG_DEFAULT),
      });
      alert(body);
    };

  return (
    <div className="mb-4 p-4 border rounded-lg shadow-sm bg-base-100">
      <h4 className="font-semibold text-md mb-3">RAG Connections</h4>

      <label className="form-control mb-2 space-y-2">
        <Label>Select Existing Connection</Label>
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

      <Button
        variant="default"
        className="w-full mt-4 hover:cursor-pointer"
        onClick={handleAddConnection}
      >
        Add Connection
      </Button>

      {/* Buttons for RAG DB actions - now using localStorage instead of API */}
      <div className="grid grid-cols-2 gap-2 mt-2">
        <Button
          variant="outline"
          size="sm"
          className="flex-grow hover:cursor-pointer"
          onClick={() => handleCreateSchema()}
        >
          Create Schema
        </Button>
        <Button
          variant="outline"
          size="sm"
          className="flex-grow hover:cursor-pointer"
          onClick={handleCheckSchemaExists}
        >
          Check Schema
        </Button>
        <Button
          variant="destructive"
          size="sm"
          className="flex-grow hover:cursor-pointer"
          onClick={handleDropSchema}
        >
          Drop Schema
        </Button>
        <Button
          variant="outline"
          size="sm"
          className="flex-grow hover:cursor-pointer"
          onClick={handleDumpConfig}
        >
          Debug
        </Button>
      </div>
    </div>
  );
};
