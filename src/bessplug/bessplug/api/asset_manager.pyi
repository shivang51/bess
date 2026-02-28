"""
Asset Manager bindings
"""
from __future__ import annotations
import bessplug.api.common
import bessplug.api.scene.renderer
__all__: list[str] = ['AssetManager', 'TextureAssetID']
class AssetManager:
    @staticmethod
    def cleanup() -> None:
        """
        Cleanup all assets in the AssetManager
        """
    @staticmethod
    def get_texture_asset(asset_id: TextureAssetID) -> bessplug.api.scene.renderer.VulkanTexture:
        """
        Get a texture asset by its AssetID
        """
    @staticmethod
    def register_texture_asset(path: str) -> TextureAssetID:
        """
        Register a texture asset and return its AssetID
        """
class TextureAssetID:
    def __init__(self, arg0: str) -> None:
        ...
    def __repr__(self) -> str:
        ...
    @property
    def paths(self) -> bessplug.api.common.StringViewArray1:
        """
        Get the paths of the texture asset
        """
