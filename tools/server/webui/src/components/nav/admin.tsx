import {
  SidebarGroup,
  SidebarGroupContent,
  SidebarGroupLabel,
  SidebarMenu,
  SidebarMenuButton,
  SidebarMenuItem,
} from '@/components/ui/sidebar';
import { MessageCirclePlus, FilePlus2, SquareTerminal } from 'lucide-react';
import { Button } from '../ui/button';
import { useNavigate } from 'react-router';
import { useRef, useState } from 'react';
import toast from 'react-hot-toast';
import { getSelectedRagConnection } from '../../Config';
import { useAppContext } from '../../utils/app.context';
import { SwitchConfigToggle } from '../SwitchSpecificConfigElem';
import { InputConfigField } from '../InputConfigField';
import { EmbeddingDisplayModal } from '../EmbeddingDisplayModal';
import StorageUtils from '@/utils/storage';

export function NavAdmin() {
  const navigate = useNavigate();
  const fileInputRef = useRef<HTMLInputElement>(null);
  const { config: gconfig } = useAppContext();

  const handleFileChange = async (
    event: React.ChangeEvent<HTMLInputElement>
  ) => {
    const files = event.target.files;
    if (files && files.length > 0) {
      const file = files[0];
      console.log('Selected file for upload:', file.name);

      const toastId = toast.loading(`Processing "${file.name}"...`); // Initial toast message

      // Define the URL for the external upload service
      const UPLOAD_SERVICE_URL =
        'https://cuyegue.secretivecomputing.tech:15000/upload';
      // const UPLOAD_SERVICE_URL = '/upload'; // Use this if you're proxying the request through your own backend

      if (file.type === 'application/pdf' || file.type.startsWith('image/')) {
        // Handles image/png and other image types
        // Handle PDF or Image files
        const formData = new FormData();
        formData.append('file', file); // 'file' is the expected field name by the /upload endpoint

        try {
          toast.loading(`Uploading "${file.name}" to OCR service...`, {
            id: toastId,
          });
          const response = await fetch(UPLOAD_SERVICE_URL, {
            method: 'POST',
            body: formData,
          });

          if (!response.ok) {
            const errorData = await response.json();
            throw new Error(
              errorData.error ||
                `Error processing the file: ${response.statusText}`
            );
          }

          const data = await response.json();
          const extractedText = data.text; // Assuming the OCR service returns 'text' field

          if (!extractedText) {
            throw new Error('OCR service did not return any text content.');
          }

          // Now, proceed with chunking the extracted text
          toast.loading(`OCR successful, chunking "${file.name}"...`, {
            id: toastId,
          });
          await sendToChunkingEndpoint(
            file.name,
            file.type,
            extractedText.length,
            extractedText,
            toastId,
            gconfig
          );
        } catch (error) {
          console.error('OCR/Upload service failed:', error);
          toast.error(
            `Failed to process "${file.name}" via OCR: ${error instanceof Error ? error.message : String(error)}`,
            { id: toastId }
          );
        } finally {
          if (fileInputRef.current) {
            fileInputRef.current.value = '';
          }
        }
      } else if (
        file.type.startsWith('text/') ||
        file.type === 'application/json' ||
        file.type ===
          'application/vnd.openxmlformats-officedocument.wordprocessingml.document'
      ) {
        // Handles .txt, .md, .json, .docx etc.
        // Original handling for text-based files
        toast.loading(`Reading and chunking "${file.name}"...`, {
          id: toastId,
        });
        const reader = new FileReader();

        reader.onload = async (e) => {
          const fileContent = e.target?.result as string; // Get the content as a string
          await sendToChunkingEndpoint(
            file.name,
            file.type,
            fileContent.length,
            fileContent,
            toastId,
            gconfig
          );
        };

        reader.onerror = (error) => {
          toast.error(`Error reading file "${file.name}": ${error}`, {
            id: toastId,
          });
          console.error('File reading error:', error);
          if (fileInputRef.current) {
            fileInputRef.current.value = '';
          }
        };

        reader.readAsText(file);
      } else {
        // Handle unsupported file types
        toast.error(
          `Unsupported file type: "${file.type}". Please upload .txt, .pdf, .docx, .md, .json, or image files.`,
          { id: toastId }
        );
        if (fileInputRef.current) {
          fileInputRef.current.value = '';
        }
        console.warn('Unsupported file type:', file.type);
      }
    }
  };

  // Helper function to encapsulate the chunking logic
  async function sendToChunkingEndpoint(
    fileName: string,
    fileType: string,
    fileLength: number,
    fileContent: string,
    toastId: string,
    gconfig: any
  ) {
    const selectedRagConnection = getSelectedRagConnection(gconfig); // Pass gconfig to the helper

    const requestBody = {
      input: fileContent,
      rag_connection: {
        // It seems 'searched_name' is not a field on your C++ backend for rag_connection
        // Removing it for now, or ensure your backend expects it.
        // searched_name: gconfig.selected_rag_connection_name,
        connection_name: selectedRagConnection.connection_name,
        user: selectedRagConnection.user,
        password: selectedRagConnection.password,
        host: selectedRagConnection.host,
        port: selectedRagConnection.port,
        name: selectedRagConnection.name,
      },
      rag_insertion_params: {
        document: {
          date: new Date().toISOString(),
          version: '1.0',
          'content-type': fileType, // Use original file type or 'text/plain' if OCR'd
          url: fileName, // Or a more meaningful URL
          length: fileLength, // Use the length of the extracted text
        },
        controller_public_key:
          '012345678901234567890123456789012345678901234567890123456789012345',
        recipient_private_key:
          '0123456789012345678901234567890123456789012345678901234567890123',
      },
    };

    try {
      const response = await fetch('/chunking', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(requestBody),
      });

      if (response.ok) {
        const result = await response.json();
        toast.success(`"${fileName}" chunked successfully!`, {
          id: toastId,
        });
        console.log('Chunking success:', result);
      } else {
        const errorData = await response.json();
        toast.error(
          `Failed to chunk "${fileName}": ${errorData.message || response.statusText}`,
          { id: toastId }
        );
        console.error('Chunking failed:', response.statusText, errorData);
      }
    } catch (error) {
      toast.error(
        `Error during chunking "${fileName}": ${error instanceof Error ? error.message : String(error)}`,
        { id: toastId }
      );
      console.error('Network or other error during chunking:', error);
    }
  }

  const handleNewDocumentClick = () => {
    fileInputRef.current?.click();
  };

  const [showEmbeddingModal, setShowEmbeddingModal] = useState(false);
  const [embeddingVector, setEmbeddingVector] = useState<number[] | null>(null);

  // New function to get embedding vector
  const provideChunkEmbeddingVector = async () => {
    const toastId = toast.loading('Getting embedding vector...');
    try {
      // The C++ backend indicates it can take aggregation_rule and aggregation_sample
      // For now, we'll send a minimal body. If you need to specify these,
      // you could add input fields for them in the UI.
      const config = StorageUtils.getConfig();
      const custom = config.custom.length ? JSON.parse(config.custom) : {};
      const aggregation_sample: number =
        custom['chunk_sz'] == undefined ? 300 : Number(custom['chunk_sz']);
      const requestBody = {
        aggregation_rule: 'average', // Example: if you want to explicitly send this
        aggregation_sample: aggregation_sample,
      };

      const response = await fetch('/compute-chunk-vector', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(requestBody),
      });

      if (!response.ok) {
        const errorData = await response.json();
        throw new Error(
          errorData.message || `HTTP error! status: ${response.status}`
        );
      }

      const data = await response.json();
      const embedding = data['chunk-embedding'];

      if (!Array.isArray(embedding) || embedding.some(isNaN)) {
        throw new Error('Invalid embedding vector received from backend.');
      }

      setEmbeddingVector(embedding);
      setShowEmbeddingModal(true); // Show the modal
      toast.success('Embedding vector retrieved!', { id: toastId });
    } catch (error) {
      console.error('Failed to get embedding vector:', error);
      toast.error(
        `Failed to get embedding vector: ${error instanceof Error ? error.message : String(error)}`,
        { id: toastId }
      );
      setEmbeddingVector(null);
      setShowEmbeddingModal(false); // Ensure modal is closed on error
    }
  };

  return (
    <SidebarGroup>
      <SidebarGroupLabel>Admin</SidebarGroupLabel>
      <SidebarGroupContent>
        <SidebarMenu>
          {/* New Conversation Button */}
          <SidebarMenuItem>
            <SidebarMenuButton asChild>
              <Button
                variant="ghost"
                className="w-full justify-start gap-2"
                onClick={() => navigate('/')}
              >
                <MessageCirclePlus className="h-4 w-4" />
                <span>New Conversation</span>
              </Button>
            </SidebarMenuButton>
          </SidebarMenuItem>
          {/* New RAG Document Button */}
          <SidebarMenuItem>
            <SidebarMenuButton asChild>
              <Button
                variant="ghost"
                className="w-full justify-start gap-2"
                onClick={handleNewDocumentClick}
              >
                <FilePlus2 className="h-4 w-4" />
                <span>New RAG Doc</span>
              </Button>
            </SidebarMenuButton>
          </SidebarMenuItem>
          {/* switch to ingest mode */}
          <SwitchConfigToggle configKey="ingest" labelText="Ingest Mode" />
          {/* switch to rendering vectors*/}
          <SidebarMenuItem>
            <SidebarMenuButton asChild>
              <Button
                variant="ghost"
                className="w-full justify-start gap-2"
                onClick={provideChunkEmbeddingVector} // This calls the function
              >
                <SquareTerminal className="h-4 w-4" /> {/* New icon */}
                <span>Get Embedding Vector</span>
              </Button>
            </SidebarMenuButton>
          </SidebarMenuItem>
          {/* Embedding Display Modal */}
          {showEmbeddingModal && embeddingVector && (
            <EmbeddingDisplayModal
              vector={embeddingVector}
              onClose={() => setShowEmbeddingModal(false)}
            />
          )}
          {/* Sampling size for the chunk*/}
          <InputConfigField
            configKey="chunk_sz"
            labelText="Chunk Size"
            type="number"
          />
          {/* Hidden file input for document upload */}
          <input
            type="file"
            ref={fileInputRef}
            onChange={handleFileChange}
            className="hidden"
            accept=".txt,.pdf,.docx,.md,.json"
          />
        </SidebarMenu>
      </SidebarGroupContent>
    </SidebarGroup>
  );
}
