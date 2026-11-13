import { useState } from 'react';
import { Button, StyleSheet, Text, View } from 'react-native';
import HomeScreen from './screens/HomeScreen';

type Screen = { name: 'home' } | { name: 'camera' };

export default function App() {
  const [screen, setScreen] = useState<Screen>({ name: 'home' });

  if (screen.name === 'camera') {
    return (
      <View style={styles.placeholder}>
        <Text>Camera screen</Text>
        <Button title="Back" onPress={() => setScreen({ name: 'home' })} />
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
