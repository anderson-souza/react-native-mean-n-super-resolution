import { useCallback, useEffect, useRef, useState } from 'react';
import { BackHandler, Button, StyleSheet, Text, View } from 'react-native';
import type { CameraRun } from './burst/types';
import CameraScreen from './screens/CameraScreen';
import HomeScreen from './screens/HomeScreen';

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
      <View style={styles.placeholder}>
        <Text>
          Output: {screen.run.result.width}×{screen.run.result.height}
        </Text>
        <Button title="Back" onPress={() => setScreen({ name: 'camera' })} />
      </View>
    );
  }
  return <HomeScreen onOpenCamera={() => setScreen({ name: 'camera' })} />;
}

const styles = StyleSheet.create({
  placeholder: {
    alignItems: 'center',
    flex: 1,
    gap: 12,
    justifyContent: 'center',
  },
});
