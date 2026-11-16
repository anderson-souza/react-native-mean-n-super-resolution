require "json"
require "digest"
require "fileutils"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))
opencv = JSON.parse(File.read(File.join(__dir__, "opencv.json")))

# why: prepare_command does not run for :path (development) pods, so the pinned
# opencv-mobile Apple package is fetched while the spec is evaluated.
# invariant: the .sha256 stamp names the archive the directory was extracted
# from; a changed opencv.json hash forces a fresh download.
opencv_pkg = opencv["packages"]["apple"]
opencv_rel = File.join(".opencv", opencv["version"], "apple")
opencv_root = File.join(__dir__, opencv_rel)
opencv_stamp = File.join(opencv_root, ".sha256")
unless File.exist?(opencv_stamp) && File.read(opencv_stamp).strip == opencv_pkg["sha256"]
  FileUtils.mkdir_p(File.dirname(opencv_root))
  archive = "#{opencv_root}.zip"
  staging = "#{opencv_root}.tmp"
  Pod::UI.puts "opencv-mobile: downloading #{opencv_pkg["url"]}" if defined?(Pod::UI)
  unless system("curl", "-fsSL", "--retry", "3", "-o", archive, opencv_pkg["url"])
    FileUtils.rm_f(archive)
    raise "opencv-mobile: download failed: #{opencv_pkg["url"]}"
  end
  actual = Digest::SHA256.file(archive).hexdigest
  unless actual == opencv_pkg["sha256"]
    FileUtils.rm_f(archive)
    raise "opencv-mobile: sha256 mismatch for #{opencv_pkg["url"]}: expected #{opencv_pkg["sha256"]}, got #{actual}"
  end
  FileUtils.rm_rf([staging, opencv_root])
  unless system("unzip", "-q", archive, "-d", staging)
    raise "opencv-mobile: extract failed: #{archive}"
  end
  FileUtils.mv(staging, opencv_root)
  File.write(opencv_stamp, opencv_pkg["sha256"])
  FileUtils.rm_f(archive)
end

Pod::Spec.new do |s|
  s.name         = "MeanNSuperResolution"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => min_ios_version_supported }
  s.source       = { :git => "https://github.com/anderson-souza/react-native-mean-n-super-resolution.git", :tag => "v#{s.version}" }

  # invariant: cpp/tests (host gtest + host TempDir) must never be compiled into the app.
  s.source_files = "ios/**/*.{h,m,mm}", "cpp/*.{h,cpp}", "cpp/sr/**/*.{h,cpp}", "cpp/platform/**/*.h"
  s.private_header_files = "ios/**/*.h"

  s.vendored_frameworks = File.join(opencv_rel, "opencv2.xcframework")
  s.frameworks = "Accelerate"

  install_modules_dependencies(s)
end
