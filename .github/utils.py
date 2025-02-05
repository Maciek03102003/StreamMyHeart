import os
import subprocess
import tarfile

def download_and_extract_tarball(repo_url, tarball_name, extracted_dir_name, target_dir):
    # Ensure the target directory exists
    os.makedirs(os.path.dirname(target_dir), exist_ok=True)

    print(f"Downloading repository tarball from {repo_url}...")
    download_opencv_cmd = ["curl", "-L", "-o", f"./{tarball_name}", repo_url]
    subprocess.run(download_opencv_cmd, check=True)

    print(f"Extracting {tarball_name}...")
    with tarfile.open(tarball_name, "r:gz") as tar:
        tar.extractall(path=os.path.dirname(target_dir))

    os.rename(os.path.join(os.path.dirname(target_dir), extracted_dir_name), target_dir)
    print(f"Repository extracted to {target_dir}")

    # Clean up the tarball
    os.remove(tarball_name)
    print(f"Removed tarball {tarball_name}")