import { useCallback, useEffect, useRef, useState } from 'react';
import { BackHandler } from 'react-native';
import type { CameraRun } from './burst/types';
import CameraScreen from './screens/CameraScreen';
import HomeScreen from './screens/HomeScreen';
import PreviewScreen from './screens/PreviewScreen';

type Screen =
  { name: 'home' } | { name: 'camera' } | { name: 'preview'; run: CameraRun };

export default function App() {
  const [screen, setScreen] = useState<Screen>({ name: 'home' });
  const cameraBusy = useRef(false);

  const onBusyChange = useCallback((busy: boolean) => {
    cameraBusy.current = busy;
  }, []);

  useEffect(() => {
    const sub = BackHandler.addEventListener('hardwareBackPress', () => {
      if (screen.name === 'preview') {
        setScreen({ name: 'camera' });
        return true;
      }
      if (screen.name === 'camera') {
        // invariant: no navigation while a burst is capturing or processing (CAM-13).
        if (!cameraBusy.current) setScreen({ name: 'home' });
        return true;
      }
      return false;
    });
    return () => sub.remove();
  }, [screen.name]);

  if (screen.name === 'camera') {
    return (
      <CameraScreen
        onBack={() => setScreen({ name: 'home' })}
        onResult={(run) => setScreen({ name: 'preview', run })}
        onBusyChange={onBusyChange}
      />
    );
  }
  if (screen.name === 'preview') {
    return (
      <PreviewScreen
        run={screen.run}
        onBack={() => setScreen({ name: 'camera' })}
      />
    );
  }
  return <HomeScreen onOpenCamera={() => setScreen({ name: 'camera' })} />;
}
