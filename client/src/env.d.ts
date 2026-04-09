declare module "react" {
  export type FormEvent<T = Element> = Event & { currentTarget: T };
  export type ReactNode = unknown;
  export type StrictModeProps = { children?: ReactNode };

  export const StrictMode: (props: StrictModeProps) => unknown;
  export function useEffect(effect: () => void | (() => void), deps?: readonly unknown[]): void;
  export function useRef<T>(initialValue: T): { current: T };
  export function useState<T>(initialValue: T): [T, (value: T) => void];

  const React: {
    StrictMode: typeof StrictMode;
  };

  export default React;
}

declare module "react-dom/client" {
  export function createRoot(container: Element | DocumentFragment): {
    render(node: unknown): void;
  };
}

declare namespace JSX {
  interface IntrinsicElements {
    [elemName: string]: any;
  }
}
