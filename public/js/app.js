// Test API call
async function testAPI() {
    try {
        const response = await fetch('/api/hello');
        const data = await response.json();
        
        const messageDiv = document.getElementById('message');
        messageDiv.className = 'card success';
        messageDiv.innerHTML = `
            <h2>API Response</h2>
            <pre>${JSON.stringify(data, null, 2)}</pre>
        `;
        
        console.log('API response:', data);
    } catch (error) {
        const messageDiv = document.getElementById('message');
        messageDiv.className = 'card error';
        messageDiv.innerHTML = `
            <h2>Error</h2>
            <p>${error.message}</p>
        `;
        
        console.error('API error:', error);
    }
}

// Run on page load
window.addEventListener('DOMContentLoaded', () => {
    console.log('Hanuman Framework - Static file demo loaded');
    testAPI();
});
