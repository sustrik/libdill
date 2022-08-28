{-
 Copyright 2019, Mokshasoft AB (mokshasoft.com)

 This software may be distributed and modified according to the terms of
 the GNU General Public License version 3. Note that NO WARRANTY is provided.
 See "LICENSE.txt" for details.
 -}
{-# LANGUAGE ForeignFunctionInterface #-}

{-
 FFI for the handles part of libdill.h
 -}
module FFI.Handles
  ( dill_hclose
  ) where

import Foreign.C

foreign import ccall "dill_hclose" dill_hclose :: CInt -> IO CInt
