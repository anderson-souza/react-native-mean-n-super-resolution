import { useState } from 'react';
import {
  Button,
  Image,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from 'react-native';
import type { CameraRun } from '../burst/types';

type Props = { run: CameraRun; onBack: () => void };

export default function PreviewScreen({ run, onBack }: Props) {
  const [showReference, setShowReference] = useState(false);
  const { result, paths, requested, elapsedMs } = run;
  const uri = showReference ? `file://${paths[0]}` : result.path;

  return (
    <ScrollView contentContainerStyle={styles.container}>
      <View style={styles.header}>
        <Button title="Back" onPress={onBack} />
        <Text style={styles.title}>Enhanced image</Text>
      </View>
      <Text testID="preview-dims">
        Output: {result.width}×{result.height}
      </Text>
      <Text testID="preview-frames">
        framesUsed: {result.framesUsed} / {requested} requested
      </Text>
      <Text testID="preview-ms">Processing: {elapsedMs} ms</Text>
      <View style={styles.toggle}>
        <Button
          title={showReference ? 'Enhanced' : '[Enhanced]'}
          onPress={() => setShowReference(false)}
        />
        <Button
          title={
            showReference ? '[Reference (frame 1)]' : 'Reference (frame 1)'
          }
          onPress={() => setShowReference(true)}
        />
      </View>
      {/* why: same box and aspect ratio for both, so the reference is shown upscaled for a direct comparison (CAM-07) */}
      <Image
        source={{ uri }}
        resizeMode="contain"
        style={[styles.image, { aspectRatio: result.width / result.height }]}
      />
      <Text selectable style={styles.path}>
        {uri}
      </Text>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    gap: 12,
    padding: 16,
    paddingTop: 48,
  },
  header: {
    alignItems: 'center',
    flexDirection: 'row',
    gap: 12,
  },
  title: {
    fontSize: 20,
    fontWeight: '600',
  },
  toggle: {
    flexDirection: 'row',
    gap: 12,
    justifyContent: 'center',
  },
  image: {
    backgroundColor: 'black',
    width: '100%',
  },
  path: {
    fontSize: 11,
  },
});
