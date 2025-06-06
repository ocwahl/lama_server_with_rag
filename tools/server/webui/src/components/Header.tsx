import { useAppContext } from '../utils/app.context';
import { Settings2 } from 'lucide-react';
import { Button } from './ui/button';

export default function Header() {
  const { setShowSettings } = useAppContext();

  return (
    <div className="flex items-center justify-end p-6 sticky top-0 z-10">
      <Button
        size="icon"
        variant="outline"
        className="hover:cursor-pointer"
        onClick={() => setShowSettings(true)}
      >
        <Settings2 className="text-gray-500" />
      </Button>
    </div>
  );
}
