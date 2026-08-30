1. **Analyze UX improvement**
   - We will improve the login and signup forms by adding a transient loading state to their submit buttons. Currently, when a user attempts to sign in or register, they click the button and there is no visual feedback while the async Firebase request resolves. This is one of Palette's favorite UX improvements ("✨ Add loading spinner to async submit button", or transient text since it's a text button).
   - This change strictly aligns with Palette's constraints (pure UX addition, no structural changes, < 50 lines).

2. **Modify `transparency-web/public/js/auth.js`**
   - Update `signupForm.addEventListener("submit", ...)`
   - Update `loginForm.addEventListener("submit", ...)`
   - Inject button state logic using `btn.dataset.originalText`.

   ```python
   # Script to update login form
   ...
   ```

3. **Verify modification**
   - Use Playwright to intercept auth request and take a screenshot of the loading state.

4. **Complete Pre Commit Steps**
   - Complete pre-commit steps to ensure proper testing, verification, review, and reflection are done.

5. **Submit**
   - Create PR with title "🎨 Palette: Add loading states to authentication forms"
