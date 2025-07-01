import React from 'react';
import { Button } from '@/components/ui/button'; // Assuming you have a Button component
import { X } from 'lucide-react'; // For the close icon

interface EmbeddingDisplayModalProps {
  vector: number[];
  onClose: () => void;
}

export function EmbeddingDisplayModal({
  vector,
  onClose,
}: EmbeddingDisplayModalProps) {
  // Display only the first 15 elements for brevity
  const displayVector = vector.slice(0, 15);
  const remainingElements = vector.length - displayVector.length;

  return (
    // Overlay
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black bg-opacity-50 p-4">
      <div className="relative w-full max-w-md rounded-lg bg-white p-6 shadow-xl dark:bg-gray-800">
        {/* Close Button */}
        <Button
          variant="ghost"
          size="icon"
          className="absolute right-4 top-4 rounded-full"
          onClick={onClose}
          aria-label="Close"
        >
          <X className="h-5 w-5 text-gray-500 dark:text-gray-400" />
        </Button>

        <h2 className="mb-4 text-xl font-semibold text-gray-900 dark:text-white">
          Embedding Vector
        </h2>

        <div className="mb-6 max-h-60 overflow-y-auto rounded-md bg-gray-50 p-4 font-mono text-sm text-gray-800 dark:bg-gray-700 dark:text-gray-200">
          <p className="whitespace-pre-wrap break-all">
            [
            {displayVector.map((val, index) => (
              <React.Fragment key={index}>
                {index > 0 && ', '}
                <span className="inline-block min-w-[60px] text-right">
                  {val.toFixed(6)}
                </span>
                {(index + 1) % 5 === 0 &&
                  index !== displayVector.length - 1 && <br />}{' '}
                {/* New line every 5 elements */}
              </React.Fragment>
            ))}
            {remainingElements > 0 &&
              `, ... (${remainingElements} more elements)`}
            ]
          </p>
        </div>

        <div className="flex justify-end">
          <Button onClick={onClose}>OK</Button>
        </div>
      </div>
    </div>
  );
}
