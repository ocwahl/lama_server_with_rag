import {
  SidebarGroup,
  SidebarGroupContent,
  SidebarGroupLabel,
  SidebarMenu,
  SidebarMenuButton,
  SidebarMenuItem,
} from '@/components/ui/sidebar';
import {
  MessageCirclePlus,
  FilePlus2,
} from 'lucide-react';
import { Button } from '../ui/button';
import { useNavigate } from 'react-router';
import { useRef } from 'react';
import toast from 'react-hot-toast';
import { getSelectedRagConnection } from '../../Config';
import { useAppContext } from '../../utils/app.context';

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

      // Store the toast ID to dismiss it later
      const toastId = toast.loading(`Uploading "${file.name}" for chunking...`);

      const reader = new FileReader();

      reader.onload = async (e) => {
        const fileContent = e.target?.result as string; // Get the content as a string
        const requestBody = {
          input: fileContent,
          rag_connection: {
            searched_name: gconfig.selected_rag_connection_name,
            connection_name: getSelectedRagConnection(gconfig).connection_name,
            user: getSelectedRagConnection(gconfig).user,
            password: getSelectedRagConnection(gconfig).password,
            host: getSelectedRagConnection(gconfig).host,
            port: getSelectedRagConnection(gconfig).port,
            name: getSelectedRagConnection(gconfig).name,
          },
          rag_insertion_params: {
            document: {
              date: new Date().toISOString(),
              version: '1.0',
              'content-type': file.type,
              url: file.name, // Or a more meaningful URL
              length: fileContent.length,
            },
            controller_public_key:
              '012345678901234567890123456789012345678901234567890123456789012345', // TODO: Replace with actual keys
            recipient_private_key:
              '0123456789012345678901234567890123456789012345678901234567890123', // TODO: Replace with actual keys
          },
        };

        try {
          const response = await fetch('/chunking', {
            method: 'POST',
            headers: {
              'Content-Type': 'application/json',
              // Add any authentication headers if required by your backend
            },
            body: JSON.stringify(requestBody),
          });

          if (response.ok) {
            const result = await response.json();
            toast.success(`"${file.name}" uploaded and chunked successfully!`, {
              id: toastId,
            });
            console.log('Upload success:', result);
          } else {
            const errorData = await response.json();
            toast.error(
              `Failed to upload "${file.name}": ${errorData.message || response.statusText}`,
              { id: toastId }
            );
            console.error('Upload failed:', response.statusText, errorData);
          }
        } catch (error) {
          toast.error(
            `Error uploading "${file.name}": ${error instanceof Error ? error.message : String(error)}`,
            { id: toastId }
          );
          console.error('Network or other error during upload:', error);
        } finally {
          // Ensure the file input is cleared regardless of success or failure
          if (fileInputRef.current) {
            fileInputRef.current.value = '';
          }
        }
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
    }
  };

  const handleNewDocumentClick = () => {
    fileInputRef.current?.click();
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
