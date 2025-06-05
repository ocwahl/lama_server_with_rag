// tools/server/webui/src/components/Sidebar.tsx

import { useEffect, useMemo, useState, useRef } from 'react';
import { classNames } from '../utils/misc';
import { Conversation } from '../utils/types';
import StorageUtils from '../utils/storage';
import { useNavigate, useParams } from 'react-router';
import {
  ArrowDownTrayIcon,
  EllipsisVerticalIcon,
  PencilIcon,
  PencilSquareIcon,
  TrashIcon,
  XMarkIcon,
  DocumentPlusIcon,
} from '@heroicons/react/24/outline';
import { BtnWithTooltips } from '../utils/common';
import { useAppContext } from '../utils/app.context';
import { getSelectedRagConnection } from '../Config';
import toast from 'react-hot-toast';
//import { config } from 'process';

export default function Sidebar() {
  const params = useParams();
  const navigate = useNavigate();
  const fileInputRef = useRef<HTMLInputElement>(null);

  const { isGenerating } = useAppContext();
  const { config: gconfig } = useAppContext();

  const [conversations, setConversations] = useState<Conversation[]>([]);
  const [currConv, setCurrConv] = useState<Conversation | null>(null);

  useEffect(() => {
    StorageUtils.getOneConversation(params.convId ?? '').then(setCurrConv);
  }, [params.convId]);

  useEffect(() => {
    const handleConversationChange = async () => {
      setConversations(await StorageUtils.getAllConversations());
    };
    StorageUtils.onConversationChanged(handleConversationChange);
    handleConversationChange();
    return () => {
      StorageUtils.offConversationChanged(handleConversationChange);
    };
  }, []);

  const groupedConv = useMemo(
    () => groupConversationsByDate(conversations),
    [conversations]
  );

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
              'Content-Type': 'application/json', // IMPORTANT: Change content type
              // Add any authentication headers if required by your backend
            },
            body: JSON.stringify(requestBody),
          });

          if (response.ok) {
            const result = await response.json();
            toast.success(`"${file.name}" uploaded and chunked successfully!`, {
              id: toastId,
            }); // Dismiss success toast
            console.log('Upload success:', result);
          } else {
            const errorData = await response.json();
            toast.error(
              `Failed to upload "${file.name}": ${errorData.message || response.statusText}`,
              { id: toastId } // Dismiss error toast
            );
            console.error('Upload failed:', response.statusText, errorData);
          }
        } catch (error) {
          toast.error(
            `Error uploading "${file.name}": ${error instanceof Error ? error.message : String(error)}`,
            { id: toastId } // Dismiss error toast
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
        }); // Dismiss error toast for file reading
        console.error('File reading error:', error);
        if (fileInputRef.current) {
          fileInputRef.current.value = '';
        }
      };

      reader.readAsText(file);
    } // This closing brace was missing. It closes the `if (files && files.length > 0)` block.
  }; // This closing brace was missing. It closes the `handleFileChange` function.

  const handleNewDocumentClick = () => {
    fileInputRef.current?.click();
  };

  return (
    <>
      <input
        id="toggle-drawer"
        type="checkbox"
        className="drawer-toggle"
        defaultChecked
      />

      <div className="drawer-side h-screen lg:h-screen z-50 lg:max-w-64">
        <label
          htmlFor="toggle-drawer"
          aria-label="close sidebar"
          className="drawer-overlay"
        ></label>
        <div className="flex flex-col bg-base-200 min-h-full max-w-64 py-4 px-4">
          <div className="flex flex-row items-center justify-between mb-4 mt-4">
            <h2 className="font-bold ml-4">Conversations</h2>

            <label htmlFor="toggle-drawer" className="btn btn-ghost lg:hidden">
              <XMarkIcon className="w-5 h-5" />
            </label>
          </div>

          <div
            className={classNames({
              'btn btn-ghost justify-start px-2': true,
              'btn-soft': !currConv,
            })}
            onClick={() => navigate('/')}
          >
            <PencilSquareIcon className="w-5 h-5" />
            new Conversation
          </div>

          <input
            type="file"
            ref={fileInputRef}
            onChange={handleFileChange}
            className="hidden"
            accept=".txt,.pdf,.docx,.md,.json"
          />
          <div
            className={classNames({
              'btn btn-ghost justify-start px-2 mt-2': true,
              'btn-soft': false,
            })}
            onClick={handleNewDocumentClick}
          >
            <DocumentPlusIcon className="w-5 h-5" />
            New RAG Doc
          </div>

          {groupedConv.map((group, i) => (
            <div key={i}>
              {group.title ? (
                <b className="btn btn-ghost btn-xs bg-none btn-disabled block text-xs text-base-content text-start px-2 mb-0 mt-6 font-bold">
                  {group.title}
                </b>
              ) : (
                <div className="h-2" />
              )}

              {group.conversations.map((conv) => (
                <ConversationItem
                  key={conv.id}
                  conv={conv}
                  isCurrConv={currConv?.id === conv.id}
                  onSelect={() => {
                    navigate(`/chat/${conv.id}`);
                  }}
                  onDelete={() => {
                    if (isGenerating(conv.id)) {
                      toast.error(
                        'Cannot delete conversation while generating'
                      );
                      return;
                    }
                    if (
                      window.confirm(
                        'Are you sure to delete this conversation?'
                      )
                    ) {
                      toast.success('Conversation deleted');
                      StorageUtils.remove(conv.id);
                      navigate('/');
                    }
                  }}
                  onDownload={() => {
                    if (isGenerating(conv.id)) {
                      toast.error(
                        'Cannot download conversation while generating'
                      );
                      return;
                    }
                    const conversationJson = JSON.stringify(conv, null, 2);
                    const blob = new Blob([conversationJson], {
                      type: 'application/json',
                    });
                    const url = URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url;
                    a.download = `conversation_${conv.id}.json`;
                    document.body.appendChild(a);
                    a.click();
                    document.body.removeChild(a);
                    URL.revokeObjectURL(url);
                  }}
                  onRename={() => {
                    if (isGenerating(conv.id)) {
                      toast.error(
                        'Cannot rename conversation while generating'
                      );
                      return;
                    }
                    const newName = window.prompt(
                      'Enter new name for the conversation',
                      conv.name
                    );
                    if (newName && newName.trim().length > 0) {
                      StorageUtils.updateConversationName(conv.id, newName);
                    }
                  }}
                />
              ))}
            </div>
          ))}
          <div className="text-center text-xs opacity-40 mt-auto mx-4 pt-8">
            Conversations are saved to browser's IndexedDB
          </div>
        </div>
      </div>
    </>
  );
}

function ConversationItem({
  conv,
  isCurrConv,
  onSelect,
  onDelete,
  onDownload,
  onRename,
}: {
  conv: Conversation;
  isCurrConv: boolean;
  onSelect: () => void;
  onDelete: () => void;
  onDownload: () => void;
  onRename: () => void;
}) {
  return (
    <div
      className={classNames({
        'group flex flex-row btn btn-ghost justify-start items-center font-normal px-2 h-9':
          true,
        'btn-soft': isCurrConv,
      })}
    >
      <div
        key={conv.id}
        className="w-full overflow-hidden truncate text-start"
        onClick={onSelect}
        dir="auto"
      >
        {conv.name}
      </div>
      <div className="dropdown dropdown-end h-5">
        <BtnWithTooltips
          // on mobile, we always show the ellipsis icon
          // on desktop, we only show it when the user hovers over the conversation item
          // we use opacity instead of hidden to avoid layout shift
          className="cursor-pointer opacity-100 md:opacity-0 group-hover:opacity-100"
          onClick={() => {}}
          tooltipsContent="More"
        >
          <EllipsisVerticalIcon className="w-5 h-5" />
        </BtnWithTooltips>
        {/* dropdown menu */}
        <ul
          tabIndex={0}
          className="dropdown-content menu bg-base-100 rounded-box z-[1] p-2 shadow"
        >
          <li onClick={onRename}>
            <a>
              <PencilIcon className="w-4 h-4" />
              Rename
            </a>
          </li>
          <li onClick={onDownload}>
            <a>
              <ArrowDownTrayIcon className="w-4 h-4" />
              Download
            </a>
          </li>
          <li className="text-error" onClick={onDelete}>
            <a>
              <TrashIcon className="w-4 h-4" />
              Delete
            </a>
          </li>
        </ul>
      </div>
    </div>
  );
}

// WARN: vibe code below

export interface GroupedConversations {
  title?: string;
  conversations: Conversation[];
}

// TODO @ngxson : add test for this function
// Group conversations by date
// - "Previous 7 Days"
// - "Previous 30 Days"
// - "Month Year" (e.g., "April 2023")
export function groupConversationsByDate(
  conversations: Conversation[]
): GroupedConversations[] {
  const now = new Date();
  const today = new Date(now.getFullYear(), now.getMonth(), now.getDate()); // Start of today

  const sevenDaysAgo = new Date(today);
  sevenDaysAgo.setDate(today.getDate() - 7);

  const thirtyDaysAgo = new Date(today);
  thirtyDaysAgo.setDate(today.getDate() - 30);

  const groups: { [key: string]: Conversation[] } = {
    Today: [],
    'Previous 7 Days': [],
    'Previous 30 Days': [],
  };
  const monthlyGroups: { [key: string]: Conversation[] } = {}; // Key format: "Month Year" e.g., "April 2023"

  // Sort conversations by lastModified date in descending order (newest first)
  // This helps when adding to groups, but the final output order of groups is fixed.
  const sortedConversations = [...conversations].sort(
    (a, b) => b.lastModified - a.lastModified
  );

  for (const conv of sortedConversations) {
    const convDate = new Date(conv.lastModified);

    if (convDate >= today) {
      groups['Today'].push(conv);
    } else if (convDate >= sevenDaysAgo) {
      groups['Previous 7 Days'].push(conv);
    } else if (convDate >= thirtyDaysAgo) {
      groups['Previous 30 Days'].push(conv);
    } else {
      const monthName = convDate.toLocaleString('default', { month: 'long' });
      const year = convDate.getFullYear();
      const monthYearKey = `${monthName} ${year}`;
      if (!monthlyGroups[monthYearKey]) {
        monthlyGroups[monthYearKey] = [];
      }
      monthlyGroups[monthYearKey].push(conv);
    }
  }

  const result: GroupedConversations[] = [];

  if (groups['Today'].length > 0) {
    result.push({
      title: undefined, // no title for Today
      conversations: groups['Today'],
    });
  }

  if (groups['Previous 7 Days'].length > 0) {
    result.push({
      title: 'Previous 7 Days',
      conversations: groups['Previous 7 Days'],
    });
  }

  if (groups['Previous 30 Days'].length > 0) {
    result.push({
      title: 'Previous 30 Days',
      conversations: groups['Previous 30 Days'],
    });
  }

  // Sort monthly groups by date (most recent month first)
  const sortedMonthKeys = Object.keys(monthlyGroups).sort((a, b) => {
    const dateA = new Date(a); // "Month Year" can be parsed by Date constructor
    const dateB = new Date(b);
    return dateB.getTime() - dateA.getTime();
  });

  for (const monthKey of sortedMonthKeys) {
    if (monthlyGroups[monthKey].length > 0) {
      result.push({ title: monthKey, conversations: monthlyGroups[monthKey] });
    }
  }

  return result;
}
