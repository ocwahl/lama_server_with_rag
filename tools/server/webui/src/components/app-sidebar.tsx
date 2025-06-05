import * as React from 'react';
import { NavHeader } from '@/components/nav/header';
import { NavAdmin } from '@/components/nav/admin';
import { NavChats } from '@/components/nav/chats';
import {
  Sidebar,
  SidebarContent,
  SidebarFooter,
  SidebarHeader,
  SidebarRail,
} from '@/components/ui/sidebar';

export function AppSidebar({ ...props }: React.ComponentProps<typeof Sidebar>) {
  return (
    <>
      <Sidebar collapsible="icon" {...props}>
        <SidebarHeader>
          <NavHeader />
        </SidebarHeader>
        <SidebarContent>
          <NavAdmin />
          <NavChats />
        </SidebarContent>
        <SidebarFooter>
          {/* Footer note */}
          <div className="mt-6 px-2 text-center text-xs text-muted-foreground">
            Conversations are saved to browser's IndexedDB
          </div>
        </SidebarFooter>
        <SidebarRail />
      </Sidebar>
    </>
  );
}
