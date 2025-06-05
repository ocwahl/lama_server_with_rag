import { HashRouter, Outlet, Route, Routes } from 'react-router';
import { SidebarInset, SidebarProvider } from '@/components/ui/sidebar';
import Header from './components/Header';
import Sidebar from './components/Sidebar';
import { AppSidebar } from './components/app-sidebar';
import { AppContextProvider, useAppContext } from './utils/app.context';
import ChatScreen from './components/ChatScreen';
import SettingDialog from './components/SettingDialog';
import { Toaster } from 'react-hot-toast';
import { ModalProvider } from './components/ModalProvider';

function App() {
  return (
    <ModalProvider>
      <HashRouter>
        <div className="flex flex-row drawer lg:drawer-open">
          <AppContextProvider>
            <Routes>
              <Route element={<AppLayout />}>
                <Route path="/chat/:convId" element={<ChatScreen />} />
                <Route path="*" element={<ChatScreen />} />
              </Route>
            </Routes>
          </AppContextProvider>
        </div>
      </HashRouter>
    </ModalProvider>
  );
}

function AppLayout() {
  const { showSettings, setShowSettings } = useAppContext();
  return (
    <SidebarProvider>
      <AppSidebar />
      <SidebarInset>
        <main
          className="flex flex-1 flex-col"
          id="main-scroll"
        >
          <Header />
          <Outlet />
        </main>
      </SidebarInset>
      {
        <SettingDialog
          show={showSettings}
          onClose={() => setShowSettings(false)}
        />
      }
      <Toaster />
    </SidebarProvider>
  );
}

export default App;
