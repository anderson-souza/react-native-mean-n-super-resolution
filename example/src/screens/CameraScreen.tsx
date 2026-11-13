import { useEffect, useMemo, useRef, useState } from 'react';
import { Button, Linking, StyleSheet, Text, View } from 'react-native';
import { Dirs, FileSystem } from 'react-native-file-access';
import { superResolve } from 'react-native-mean-n-super-resolution';
import {
  Camera,
  getCameraDevice,
  useCameraPermission,
  VisionCamera,
  type CameraDeviceFactory,
  type CameraRef,
} from 'react-native-vision-camera';
import { createBurstRunner } from '../burst/burstRunner';
import {
  DEFAULT_FRAME_COUNT,
  FRAME_COUNTS,
  type FrameCount,
} from '../burst/frameCounts';
import type { BurstEvent, CameraRun } from '../burst/types';

type Status =
  | { kind: 'idle' }
  | { kind: 'capturing'; captured: number; total: FrameCount }
  | { kind: 'processing' };

type Props = {
  onBack: () => void;
  onResult: (run: CameraRun) => void;
  onBusyChange: (busy: boolean) => void;
};

async function deleteFiles(paths: string[]) {
  for (const path of paths) {
    if (await FileSystem.exists(path)) await FileSystem.unlink(path);
  }
}

export default function CameraScreen({
  onBack,
  onResult,
  onBusyChange,
}: Props) {
  const { hasPermission, canRequestPermission, requestPermission } =
    useCameraPermission();
  const [factory, setFactory] = useState<CameraDeviceFactory | null>(null);
  const [factoryError, setFactoryError] = useState<string | null>(null);
  const cameraRef = useRef<CameraRef>(null);
  const [ready, setReady] = useState(false);
  const [count, setCount] = useState<FrameCount>(DEFAULT_FRAME_COUNT);
  const [status, setStatus] = useState<Status>({ kind: 'idle' });
  const [error, setError] = useState<string | null>(null);
  const mounted = useRef(true);
  const burstId = useRef(0);

  useEffect(() => {
    mounted.current = true;
    return () => {
      mounted.current = false;
    };
  }, []);

  useEffect(() => {
    if (!hasPermission && canRequestPermission) requestPermission();
  }, [hasPermission, canRequestPermission, requestPermission]);

  useEffect(() => {
    // why: useCameraDevice returns undefined both while loading and when no
    // camera exists; holding the factory tells the two apart (CAM-12).
    VisionCamera.createDeviceFactory().then(
      (f) => mounted.current && setFactory(f),
      (e: unknown) =>
        mounted.current &&
        setFactoryError(e instanceof Error ? e.message : String(e))
    );
  }, []);

  const runner = useMemo(
    () =>
      createBurstRunner({
        takeFrame: async (i) => {
          const camera = cameraRef.current;
          if (!camera) throw new Error('Camera is not mounted');
          // why: a fresh id per burst keeps file names unique, so <Image> never shows a cached old frame.
          if (i === 0) burstId.current = Date.now();
          const image = await camera.takeSnapshot();
          const path = `${Dirs.CacheDir}/burst-${burstId.current}-${i}.jpg`;
          await image.saveToFileAsync(path, 'jpg', 95);
          return path;
        },
        deleteFiles,
        superResolve: (paths) => superResolve(paths),
        now: () => Date.now(),
      }),
    []
  );

  const busy = status.kind !== 'idle';
  useEffect(() => onBusyChange(busy), [busy, onBusyChange]);

  const onCapture = async () => {
    const pending = runner.start(count, (e: BurstEvent) => {
      if (!mounted.current) return;
      setStatus(
        e.type === 'progress'
          ? { kind: 'capturing', captured: e.captured, total: e.total }
          : { kind: 'processing' }
      );
    });
    if (!pending) return;
    setError(null);
    setStatus({ kind: 'capturing', captured: 0, total: count });
    const outcome = await pending;
    if (outcome.ok) {
      const { run } = outcome;
      console.log(
        `[camera-burst] ok N=${run.requested} out=${run.result.width}x${run.result.height} framesUsed=${run.result.framesUsed} ms=${run.elapsedMs} ref=${run.paths[0]}`
      );
    } else {
      console.log(`[camera-burst] ${outcome.stage} failed: ${outcome.message}`);
    }
    if (!mounted.current) return;
    setStatus({ kind: 'idle' });
    if (outcome.ok) {
      onResult(outcome.run);
    } else {
      setError(
        outcome.stage === 'capture'
          ? `Capture failed: ${outcome.message}`
          : outcome.message
      );
    }
  };

  const device = factory ? getCameraDevice(factory, 'back') : undefined;

  let body;
  if (!hasPermission && !canRequestPermission) {
    body = (
      <View style={styles.center}>
        <Text>Camera permission denied</Text>
        <Button title="Open settings" onPress={() => Linking.openSettings()} />
      </View>
    );
  } else if (!hasPermission) {
    body = <Text style={styles.center}>Requesting camera permission…</Text>;
  } else if (factoryError) {
    body = <Text style={styles.error}>{factoryError}</Text>;
  } else if (!factory) {
    body = <Text style={styles.center}>Loading camera…</Text>;
  } else if (!device) {
    body = <Text style={styles.center}>No camera available</Text>;
  } else {
    body = (
      <>
        {/* invariant: fixed-size box so snapshot pixel size stays constant during a burst */}
        <View style={styles.cameraBox}>
          <Camera
            ref={cameraRef}
            style={StyleSheet.absoluteFill}
            device={device}
            isActive={true}
            resizeMode="cover"
            onPreviewStarted={() => setReady(true)}
            onPreviewStopped={() => setReady(false)}
          />
        </View>
        <View style={styles.counts}>
          {FRAME_COUNTS.map((n) => (
            <Button
              key={n}
              title={n === count ? `[${n}]` : String(n)}
              onPress={() => setCount(n)}
              disabled={busy}
            />
          ))}
        </View>
        <Button
          title={`Capture ${count} frames`}
          onPress={onCapture}
          disabled={!ready || busy}
        />
      </>
    );
  }

  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <Button title="Back" onPress={onBack} disabled={busy} />
        <Text style={styles.title}>Camera burst</Text>
      </View>
      {body}
      <Text style={styles.status} testID="burst-status">
        {status.kind === 'capturing'
          ? `${status.captured}/${status.total}`
          : status.kind === 'processing'
            ? 'Processing…'
            : ''}
      </Text>
      {error && (
        <Text selectable style={styles.error} testID="burst-error">
          {error}
        </Text>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
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
  center: {
    alignItems: 'center',
    gap: 12,
    textAlign: 'center',
  },
  cameraBox: {
    aspectRatio: 3 / 4,
    backgroundColor: 'black',
    overflow: 'hidden',
    width: '100%',
  },
  counts: {
    flexDirection: 'row',
    gap: 12,
    justifyContent: 'center',
  },
  status: {
    fontSize: 16,
    minHeight: 22,
    textAlign: 'center',
  },
  error: {
    color: 'crimson',
  },
});
