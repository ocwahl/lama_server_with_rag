import { useState } from 'react'; // Removed useCallback as it's not strictly needed for focus here
import StorageUtils from '../utils/storage';
import { Label } from '@/components/ui/label';
import { Switch } from '@/components/ui/switch';
import { SidebarMenuItem } from '@/components/ui/sidebar';

interface SwitchConfigToggleProps {
  configKey: string; // The key in config.custom to control (e.g., "stream", "ingest", "reranking")
  labelText: string; // The text to display next to the switch (e.g., "Stream", "Use RERANKING")
}

export function SwitchConfigToggle({
  configKey,
  labelText,
}: SwitchConfigToggleProps) {
  // Initialize state using a function for lazy initial state
  const [isOn, setIsOn] = useState(() => {
    const config = StorageUtils.getConfig();
    try {
      const custom = config.custom.length ? JSON.parse(config.custom) : {};
      // Ensure the value from storage is treated as a boolean, default to false if undefined
      return !!custom[configKey];
    } catch (e) {
      console.error(`Error parsing custom config for key '${configKey}':`, e);
      return false;
    }
  });

  const handleCheckedChange = (newValue: boolean) => {
    // Optimistically update UI
    setIsOn(newValue);

    // Update storage
    const config = StorageUtils.getConfig();
    const custom = config.custom.length ? JSON.parse(config.custom) : {};
    custom[configKey] = newValue; // Use the dynamic configKey
    config.custom = JSON.stringify(custom);
    StorageUtils.setConfig(config);
  };

  return (
    <SidebarMenuItem>
      <div className="flex items-center justify-between mb-4">
        <Label htmlFor={`switch-${configKey}`} className="cursor-pointer">
          {labelText}
        </Label>
        <Switch
          id={`switch-${configKey}`}
          checked={isOn}
          onCheckedChange={handleCheckedChange}
        />
      </div>
    </SidebarMenuItem>
  );
}
