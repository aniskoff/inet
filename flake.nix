{
  description = "Traffic engineering";

  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-21.11";
  inputs.nur.url = "github:pschuprikov/nur-packages";
  inputs.nur.inputs.nixpkgs.follows = "nixpkgs";

  outputs = { self, flake-utils, nur, nixpkgs }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        nurpkgs = nur.packages.${system};
      in {
        devShell = pkgs.mkShell {
          buildInputs = [
            pkgs.llvmPackages_12.clang
            (nurpkgs.omnetpp_6_0.override { cppStandard = "c++17"; })
          ];
        };
      });
}
