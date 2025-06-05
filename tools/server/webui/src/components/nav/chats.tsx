import * as React from 'react';
import {
  SidebarGroup,
  SidebarGroupContent,
  SidebarGroupLabel,
  SidebarMenu,
  SidebarMenuButton,
  SidebarMenuItem,
  SidebarMenuAction,
  useSidebar,
} from '@/components/ui/sidebar';
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from '@/components/ui/dropdown-menu';
import { NavLink, useNavigate, useParams } from 'react-router';
import {
  MoreHorizontal,
  Trash2,
  Download,
  PenSquare
} from 'lucide-react';
import { useEffect, useMemo, useState } from 'react';
import { Conversation } from '../../utils/types';
import StorageUtils from '../../utils/storage';
import { useAppContext } from '../../utils/app.context';
import toast from 'react-hot-toast';


// Group conversations by date
export interface GroupedConversations {
  title?: string;
  conversations: Conversation[];
}

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

export function NavChats({
  ...props
}: React.ComponentPropsWithoutRef<typeof SidebarGroup>) {
  const { isMobile } = useSidebar();
  const params = useParams();
  const navigate = useNavigate();

  const { isGenerating } = useAppContext();

  const [conversations, setConversations] = useState<Conversation[]>([]);
  const [currConv, setCurrConv] = useState<Conversation | null>(null);

  useEffect(() => {
    StorageUtils.getOneConversation(params.id ?? '').then(setCurrConv);
  }, [params.id]);

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

  return (
    <SidebarGroup {...props}>
      <SidebarGroupLabel>Chats</SidebarGroupLabel>
      <SidebarGroupContent>
        <SidebarMenu>

            {/* Group conversations by date */}
          {groupedConv.map((group, i) => (
            <React.Fragment key={i}>
              {/* Group title */}
              {group.title && (
                <div className="px-2 pt-6 pb-1 text-xs font-semibold text-foreground/70">
                  {group.title}
                </div>
              )}

              {/* Conversations in this group */}
              {group.conversations.map((conv) => (
                <SidebarMenuItem key={conv.id}>
                  <SidebarMenuButton asChild>
                    <NavLink
                      to={`/chat/${conv.id}`}
                      className={({ isActive }) =>
                        isActive ? 'bg-sidebar-accent' : ''
                      }
                    >
                      <span className="truncate">{conv.name}</span>
                    </NavLink>
                  </SidebarMenuButton>
                  <DropdownMenu>
                    <DropdownMenuTrigger asChild>
                      <SidebarMenuAction showOnHover>
                        <MoreHorizontal className="h-4 w-4" />
                        <span className="sr-only">More</span>
                      </SidebarMenuAction>
                    </DropdownMenuTrigger>
                    <DropdownMenuContent
                      className="w-48"
                      side={isMobile ? 'bottom' : 'right'}
                      align={isMobile ? 'end' : 'start'}
                    >
                      <DropdownMenuItem
                        onClick={() => {
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
                            StorageUtils.updateConversationName(
                              conv.id,
                              newName
                            );
                          }
                        }}
                      >
                        <PenSquare className="mr-2 h-4 w-4 text-muted-foreground" />
                        <span>Rename</span>
                      </DropdownMenuItem>
                      <DropdownMenuItem
                        onClick={() => {
                          if (isGenerating(conv.id)) {
                            toast.error(
                              'Cannot download conversation while generating'
                            );
                            return;
                          }

                          StorageUtils.getOneConversation(conv.id).then(
                            (conversation) => {
                              if (!conversation) return;

                              const conversationJson = JSON.stringify(
                                conversation,
                                null,
                                2
                              );
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
                            }
                          );
                        }}
                      >
                        <Download className="mr-2 h-4 w-4 text-muted-foreground" />
                        <span>Download</span>
                      </DropdownMenuItem>
                      <DropdownMenuItem
                        onClick={() => {
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
                      >
                        <Trash2 className="mr-2 h-4 w-4 text-red-500" />
                        <span className="text-red-500">Delete</span>
                      </DropdownMenuItem>
                    </DropdownMenuContent>
                  </DropdownMenu>
                </SidebarMenuItem>
              ))}
            </React.Fragment>
          ))}
        </SidebarMenu>
      </SidebarGroupContent>
    </SidebarGroup>
  );
}
