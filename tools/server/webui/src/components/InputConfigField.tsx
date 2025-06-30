import React, { useState } from 'react';
import StorageUtils from '../utils/storage';
import { Label } from '@/components/ui/label';
import { Input } from '@/components/ui/input'; // Assuming you have an Input component
import { SidebarMenuItem } from '@/components/ui/sidebar'; // Assuming this component is for the sidebar

// Placeholder for your CONFIG_INFO and CONFIG_DEFAULT.
// You should replace these with your actual definitions.
const CONFIG_INFO: Record<string, string> = {
  exampleInputKey: 'This is an example help message for the input field.',
  anotherInputKey: 'Enter a numerical value for this setting.',
};

const CONFIG_DEFAULT: Record<string, any> = {
  exampleInputKey: 'default_value',
  anotherInputKey: 123,
};

interface InputConfigFieldProps {
  configKey: string; // The key in config.custom to control (e.g., "modelName", "apiKey")
  labelText: string; // The text to display as the label for the input
  type?: 'text' | 'number' | 'password'; // Type of the input field
}

export function InputConfigField({
  configKey,
  labelText,
  type = 'text', // Default to text input
}: InputConfigFieldProps) {
  // Initialize state using a function for lazy initial state
  const [inputValue, setInputValue] = useState(() => {
    const config = StorageUtils.getConfig();
    try {
      const custom = config.custom.length ? JSON.parse(config.custom) : {};
      // Return the stored value, or an empty string if not found/defined
      return custom[configKey] !== undefined ? String(custom[configKey]) : '';
    } catch (e) {
      console.error(`Error parsing custom config for key '${configKey}':`, e);
      return ''; // Default to empty string on error
    }
  });

  const helpMsg = CONFIG_INFO[configKey];
  const defaultValue = CONFIG_DEFAULT[configKey];

  const handleInputChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    const newValue = event.target.value;
    // Optimistically update UI
    setInputValue(newValue);

    // Update storage
    const config = StorageUtils.getConfig();
    const custom = config.custom.length ? JSON.parse(config.custom) : {};
    custom[configKey] = newValue; // Use the dynamic configKey
    config.custom = JSON.stringify(custom);
    StorageUtils.setConfig(config);
  };

  return (
    <SidebarMenuItem>
      {' '}
      {/* Wrap in SidebarMenuItem if it's meant for the sidebar */}
      <div className="mb-4 space-y-2">
        <div className="flex flex-col gap-2">
          <Label htmlFor={`input-${configKey}`}>{labelText}</Label>
          {helpMsg && (
            <div className="hidden md:block text-sm text-muted-foreground">
              {helpMsg}
            </div>
          )}
        </div>
        {helpMsg && (
          <div className="block md:hidden mb-1">
            <p className="text-xs text-muted-foreground">{helpMsg}</p>
          </div>
        )}
        <Input
          id={`input-${configKey}`}
          type={type}
          placeholder={
            defaultValue !== undefined
              ? `Default: ${defaultValue}`
              : 'Enter value'
          }
          value={inputValue}
          onChange={handleInputChange}
        />
      </div>
    </SidebarMenuItem>
  );
}
